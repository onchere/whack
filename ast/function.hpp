/**
 * Copyright 2018 Onchere Bironga
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
#ifndef WHACK_FUNCTION_HPP
#define WHACK_FUNCTION_HPP

#pragma once

#include "args.hpp"
#include "ast.hpp"
#include "body.hpp"
#include "tags.hpp"
#include "typelist.hpp"
#include <folly/Likely.h>
#include <folly/ScopeGuard.h>
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

inline static llvm::FunctionType*
getFuncType(const llvm::Module* const module, llvm::Type* const returnType,
            const Args* const args = nullptr) {
  if (!args) {
    return llvm::FunctionType::get(returnType, false);
  }
  return llvm::FunctionType::get(returnType, args->types(module),
                                 args->variadic());
}

static llvm::Expected<llvm::FunctionType*>
getFuncType(const llvm::Module* const module,
            const TypeList* const returnTypeList,
            const Args* const args = nullptr) {
  auto returnType = Type::getReturnType(
      module->getContext(),
      returnTypeList ? std::optional{returnTypeList->codegen(module)}
                     : std::nullopt,
      {});
  if (!returnType) {
    return returnType.takeError();
  }
  return getFuncType(module, *returnType, args);
}

static llvm::Function* changeFuncReturnType(llvm::Function* const func,
                                            llvm::Type* const newReturnType) {
  const auto type = llvm::FunctionType::get(
      newReturnType, func->getFunctionType()->params(), func->isVarArg());
  const auto newFunc = llvm::Function::Create(
      type, llvm::Function::ExternalLinkage, "", func->getParent());
  newFunc->takeName(func);
  newFunc->stealArgumentListFrom(*func);
  newFunc->setAttributes(func->getAttributes());
  newFunc->getBasicBlockList().splice(newFunc->begin(),
                                      func->getBasicBlockList());
  func->eraseFromParent();
  return newFunc;
}

static llvm::Expected<llvm::Type*>
deduceFuncReturnType(const llvm::Function* const func,
                     const mpc_state_t state) {
  llvm::Type* deduced{nullptr};
  for (const auto& block : *func) {
    for (const auto& inst : block) {
      if (llvm::isa<llvm::ReturnInst>(inst)) {
        const auto returnValue =
            llvm::cast<llvm::ReturnInst>(&inst)->getReturnValue();
        const auto returnType =
            returnValue ? returnValue->getType() : BasicTypes["void"];
        if (deduced) {
          if (deduced != returnType) {
            return error("type error: conflicting return "
                         "types in function `{}` at line {}",
                         func->getName().str(), state.row + 1);
          }
        } else {
          deduced = returnType;
        }
      }
    }
  }
  return deduced;
};

static llvm::Function*
bindFirstFuncArgument(llvm::IRBuilder<>& builder, llvm::Function* const func,
                      llvm::Value* const firstArgument,
                      llvm::FunctionType* const newFunctionType) {
  assert(firstArgument->getType()->isPointerTy()); // @todo
  if (!func->hasParamAttribute(0, llvm::Attribute::Nest)) {
    func->addParamAttr(0, llvm::Attribute::Nest);
  }
  const static auto charPtrTy = BasicTypes["char"]->getPointerTo(0);
  const auto module = builder.GetInsertBlock()->getModule();
  const auto tramp = builder.CreateCall(module->getOrInsertFunction(
      "__builtin_virtual_alloc", llvm::FunctionType::get(charPtrTy, false)));
  SCOPE_EXIT {
    builder.CreateCall(
        module->getOrInsertFunction(
            "__builtin_virtual_free",
            llvm::FunctionType::get(BasicTypes["void"], charPtrTy, false)),
        tramp);
  };
  const auto initFunc = module->getOrInsertFunction(
      "llvm.init.trampoline",
      llvm::FunctionType::get(BasicTypes["void"],
                              {charPtrTy, charPtrTy, charPtrTy}, false));
  builder.CreateCall(initFunc,
                     {tramp, builder.CreateBitCast(func, charPtrTy),
                      builder.CreateBitCast(firstArgument, charPtrTy)});
  const auto adjustFunc = module->getOrInsertFunction(
      "llvm.adjust.trampoline",
      llvm::FunctionType::get(charPtrTy, charPtrTy, false));
  const auto newFunc = builder.CreateBitCast(
      builder.CreateCall(adjustFunc, tramp), newFunctionType->getPointerTo(0));
  return llvm::cast<llvm::Function>(newFunc);
}

class Function final : public AST {
public:
  explicit Function(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[1]->contents} {
    auto endIdx = 4;

    if (getOutermostAstTag(ast->children[3]) == "args") {
      args_ = std::make_unique<Args>(ast->children[3]);
      ++endIdx;
    }

    if (getOutermostAstTag(ast->children[endIdx]) == "typelist") {
      returnTypeList_ = std::make_unique<TypeList>(ast->children[endIdx]);
    }

    if (getOutermostAstTag(ast->children[endIdx]) != "body") {
      ++endIdx;
    }

    if (getOutermostAstTag(ast->children[endIdx]) == "tags") {
      tags_ = std::make_unique<Tags>(ast->children[endIdx]);
      ++endIdx;
    }

    body_ = std::make_unique<Body>(ast->children[endIdx]);
  }

  llvm::Error codegen(llvm::Module* const module) const {
    const auto returns = returnTypeList_ ? returnTypeList_.get() : nullptr;
    const auto args = args_ ? args_.get() : nullptr;
    auto type = getFuncType(module, returns, args);
    if (!type) {
      return type.takeError();
    }

    const auto func = llvm::Function::Create(
        *type, llvm::Function::ExternalLinkage, name_, module);
    if (args_) {
      const auto names = args_->names();
      for (size_t i = 0; i < names.size(); ++i) {
        func->arg_begin()[i].setName(names[i]);
      }
      for (size_t i = 0; i < func->arg_size(); ++i) {
        if (!args_->arg(i).mut) {
          func->addParamAttr(i, llvm::Attribute::ReadOnly);
        }
      }
    }

    auto entry = llvm::BasicBlock::Create(func->getContext(), "entry", func);
    llvm::IRBuilder<> builder{entry};
    if (auto err = body_->codegen(builder)) {
      return err;
    }

    // we delete trailing empty blocks // @todo: Run Pass?
    if (func->size() > 1) {
      for (auto b = ++func->begin(); b != func->end(); ++b) {
        auto& block = *b;
        if (block.empty()) {
          if (auto pred = block.getSinglePredecessor()) {
            pred->eraseFromParent();
          }
        }
      }
      // if (func->size() > 1 && func->back().empty()) {
      //   func->back().eraseFromParent();
      // }
      builder.SetInsertPoint(&func->back());
    }

    if (func->back().empty() ||
        !llvm::isa<llvm::ReturnInst>(func->back().back())) {
      if (func->getReturnType() != BasicTypes["void"]) {
        return error("expected function `{}` to have a return "
                     "value at line {}",
                     name_, state_.row + 1);
      } else {
        builder.CreateRetVoid();
      }
    }

    if (auto err = body_->runScopeExit(builder)) {
      return err;
    }

    auto deduced = deduceFuncReturnType(func, state_);
    if (!deduced) {
      return deduced.takeError();
    }

    // we replace func's type if we used return type deduction
    if (func->getReturnType() == BasicTypes["auto"]) {
      (void)changeFuncReturnType(func, *deduced);
      return llvm::Error::success();
    }

    if (*deduced != func->getReturnType()) {
      return error("function `{}` returns an invalid type "
                   "at line {}",
                   name_, state_.row + 1);
    }
    return llvm::Error::success();
  }

private:
  const mpc_state_t state_;
  const std::string name_;
  std::unique_ptr<Args> args_;
  std::unique_ptr<TypeList> returnTypeList_;
  std::unique_ptr<Tags> tags_;
  std::unique_ptr<Body> body_;
};

} // end namespace whack::ast

#endif // WHACK_FUNCTION_HPP
