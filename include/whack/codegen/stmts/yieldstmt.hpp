/**
 * Copyright 2018-present Onchere Bironga
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef WHACK_YIELDSTMT_HPP
#define WHACK_YIELDSTMT_HPP

#pragma once

namespace whack::codegen::stmts {

struct CoroutineInfo {
  llvm::Value* const ID;
  const std::optional<llvm::Value*> promiseHandle;
  llvm::Value* const handleAlloc;
  llvm::Value* const handle;
};

// @todo Check if we already have some other suspension point, probe & return
// that coro info.
static auto
newCoroutine(llvm::IRBuilder<>& builder,
             std::optional<llvm::Type*> promiseType = std::nullopt) {
  const auto current = builder.GetInsertBlock();
  const auto func = current->getParent();
  auto& ctx = func->getContext();
  const auto module = current->getModule();

  const auto coroEntry = llvm::BasicBlock::Create(ctx, "coroEntry", func);
  coroEntry->moveAfter(current);
  builder.CreateBr(coroEntry);
  builder.SetInsertPoint(coroEntry);

  const static auto charPtrTy =
      llvm::cast<llvm::Type>(BasicTypes["char"]->getPointerTo(0));
  const auto tokenTy = llvm::Type::getTokenTy(module->getContext());

  const auto coroIDFunc = module->getOrInsertFunction(
      "llvm.coro.id",
      llvm::FunctionType::get(
          tokenTy, {BasicTypes["int"], charPtrTy, charPtrTy, charPtrTy},
          false));

  llvm::Value* ID;
  std::optional<llvm::Value*> promiseHandle{std::nullopt};
  using namespace expressions::factors;
  if (promiseType) {
    promiseHandle =
        builder.CreateAlloca(promiseType.value(), 0, nullptr, "coro.promise");
    ID = builder.CreateCall(
        coroIDFunc,
        {Integral::get(32), // 0
         builder.CreateBitCast(promiseHandle.value(), charPtrTy),
         NullPtr::get(), NullPtr::get()},
        "coro.ID");
  } else {
    ID = builder.CreateCall(
        coroIDFunc,
        {Integral::get(0), NullPtr::get(), NullPtr::get(), NullPtr::get()},
        "coro.ID");
  }

  const auto needsAlloc = builder.CreateCall(
      module->getOrInsertFunction(
          "llvm.coro.alloc",
          llvm::FunctionType::get(BasicTypes["bool"], tokenTy, false)),
      ID);

  const auto needsAllocBlock =
      llvm::BasicBlock::Create(ctx, "needsAlloc", func);
  const auto cont = llvm::BasicBlock::Create(ctx, "cont", func);
  builder.CreateCondBr(needsAlloc, needsAllocBlock, cont);

  builder.SetInsertPoint(needsAllocBlock);
  const auto sizeFunc = module->getOrInsertFunction(
      "llvm.coro.size.i64",
      llvm::FunctionType::get(BasicTypes["int64"], false));
  const auto size = builder.CreateCall(sizeFunc, llvm::None, "coro.size");
  const auto handleAlloc = llvm::CallInst::CreateMalloc(
      coroEntry, BasicTypes["int64"], BasicTypes["char"], size, nullptr,
      nullptr, "coro.handleAlloc");
  builder.Insert(handleAlloc);
  builder.CreateBr(cont);
  needsAllocBlock->moveAfter(coroEntry);

  builder.SetInsertPoint(cont);
  const auto mem = builder.CreatePHI(charPtrTy, 2);
  mem->addIncoming(NullPtr::get(), coroEntry);
  mem->addIncoming(handleAlloc, needsAllocBlock);
  const auto handleFunc = module->getOrInsertFunction(
      "llvm.coro.begin",
      llvm::FunctionType::get(charPtrTy, {tokenTy, charPtrTy}, false));
  const auto handle = builder.CreateCall(handleFunc, {ID, mem}, "coro.handle");
  cont->moveAfter(needsAllocBlock);
  return CoroutineInfo{ID, promiseHandle, mem, handle};
}

// @ Hasn't been tested, LIKELY doesn't work
class YieldStmt final : public Stmt {
public:
  explicit YieldStmt(const mpc_ast_t* const ast)
      : Stmt(kYield), state_{ast->state} {
    if (ast->children_num == 3) {
      exprList_ = expressions::getExprList(ast->children[1]);
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto promiseValues = expressions::getExprValues(builder, exprList_);
    if (!promiseValues) {
      return promiseValues.takeError();
    }
    const auto promise = *promiseValues;
    std::optional<llvm::Type*> promiseType{std::nullopt};
    const auto func = builder.GetInsertBlock()->getParent();
    const auto module = func->getParent();
    if (!promise.empty()) {
      const auto promiseTypeName =
          format("::promise::{}", func->getName().data());
      if (promise.size() == 1) {
        promiseType = promise.back()->getType();
        // using ::promise::{func->getName()} = promiseType;
        if (auto err = elements::Alias::add(module, promiseTypeName,
                                            promiseType.value())) {
          return err;
        }
      } else {
        small_vector<llvm::Type*> types;
        for (const auto val : promise) {
          types.push_back(val->getType());
        }
        promiseType = llvm::StructType::create(module->getContext(), types,
                                               promiseTypeName);
      }
    }

    const auto coroInfo = newCoroutine(builder, promiseType);
    if (!promise.empty()) {
      if (promiseType.value()->isStructTy()) {
        const auto aggPromise = coroInfo.promiseHandle.value();
        for (size_t i = 0; i < promise.size(); ++i) {
          const auto ptr =
              builder.CreateStructGEP(promiseType.value(), aggPromise, i, "");
          builder.CreateStore(promise[i], ptr);
        }
      } else {
        builder.CreateStore(promise.back(), coroInfo.promiseHandle.value());
      }
    }

    const auto current = builder.GetInsertBlock();
    const auto tokenTy = llvm::Type::getTokenTy(module->getContext());

    const auto suspendFunc = module->getOrInsertFunction(
        "llvm.coro.suspend",
        llvm::FunctionType::get(BasicTypes["char"],
                                {tokenTy, BasicTypes["bool"]}, false));
    const auto suspend = builder.CreateCall(
        suspendFunc, {llvm::ConstantTokenNone::get(module->getContext()),
                      builder.getInt1(0)});
    auto& ctx = func->getContext();
    const auto cleanupBlock = llvm::BasicBlock::Create(ctx, "cleanup", func);
    const auto freeCoro = llvm::BasicBlock::Create(ctx, "freeCoro", func);
    const auto suspendBlock = llvm::BasicBlock::Create(ctx, "suspend", func);
    const auto switcher = builder.CreateSwitch(suspend, suspendBlock, 2);

    using namespace expressions::factors;
    switcher->addCase(Integral::get<llvm::ConstantInt>(0, BasicTypes["char"]),
                      current);
    switcher->addCase(Integral::get<llvm::ConstantInt>(1, BasicTypes["char"]),
                      cleanupBlock);

    builder.SetInsertPoint(cleanupBlock);
    const static auto charPtrTy = BasicTypes["char"]->getPointerTo(0);
    const auto handleAlloc = builder.CreateCall(
        module->getOrInsertFunction(
            "llvm.coro.free",
            llvm::FunctionType::get(charPtrTy, {tokenTy, charPtrTy}, false)),
        {coroInfo.ID, coroInfo.handle});
    builder.CreateCondBr(builder.CreateIsNotNull(handleAlloc), freeCoro,
                         suspendBlock);
    cleanupBlock->moveAfter(current);

    builder.SetInsertPoint(freeCoro);
    builder.Insert(llvm::CallInst::CreateFree(handleAlloc, freeCoro));
    builder.CreateBr(suspendBlock);
    freeCoro->moveAfter(cleanupBlock);

    builder.SetInsertPoint(suspendBlock);
    builder.CreateCall(
        module->getOrInsertFunction(
            "llvm.coro.end",
            llvm::FunctionType::get(BasicTypes["bool"],
                                    {charPtrTy, BasicTypes["bool"]}, false)),
        {coroInfo.handle, builder.getInt1(0)});
    builder.CreateRet(coroInfo.handle);
    suspendBlock->moveAfter(freeCoro);

    // We indicate func is a coroutine
    const auto coroMD = module->getOrInsertNamedMetadata("coroutines");
    coroMD->addOperand(
        llvm::MDBuilder{module->getContext()}.createTBAARoot(func->getName()));
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kYield;
  }

private:
  const mpc_state_t state_;
  small_vector<expr_t> exprList_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_YIELDSTMT_HPP
