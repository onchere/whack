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
#ifndef WHACK_NEWEXPR_HPP
#define WHACK_NEWEXPR_HPP

#pragma once

#include <llvm/IR/MDBuilder.h>

namespace whack::codegen::expressions::factors {

class NewExpr final : public Factor {
public:
  explicit constexpr NewExpr(const mpc_ast_t* const ast) noexcept
      : Factor(kNewExpr), ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto block = builder.GetInsertBlock();
    const auto module = block->getParent()->getParent();
    const auto initialize = [&](llvm::Value* const ptr,
                                const mpc_ast_t* const init,
                                llvm::Type* const type) -> llvm::Error {
      auto initializer = Initializer{init}.codegen(builder, type);
      if (!initializer) {
        return initializer.takeError();
      }
      builder.CreateStore(*initializer, ptr);
      return llvm::Error::success();
    };

    const auto allocate = [&](llvm::Type* const type,
                              llvm::Value* const allocSize) -> llvm::Value* {
      const auto call = llvm::CallInst::CreateMalloc(
          block, BasicTypes["int64"], type, allocSize, nullptr, nullptr, "");
      builder.Insert(call);
      // discard "malloccall" name (we rely on IR value names)
      call->getOperand(0)->setName("::new");
      return call;
    };

    llvm::Value* mem;
    if (std::string_view(ast_->children[1]->contents) == "(") {
      auto expr = getExpressionValue(ast_->children[2])->codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      if (llvm::isa<llvm::AllocaInst>(*expr)) {
        mem = builder.CreateLoad(*expr);
      } else {
        auto m = getLoadedValue(builder, *expr);
        if (!m) {
          return m.takeError();
        }
        mem = *m;
      }
      const auto memType = mem->getType();
      auto type = types::Type{ast_->children[4]}.codegen(builder);
      if (!type) {
        return type.takeError();
      }
      if (memType->isIntegerTy()) {
        const auto allocSize = builder.CreateMul(
            Integral::get(types::Type::getByteSize(module, *type),
                          BasicTypes["int64"]),
            builder.CreateIntCast(mem, BasicTypes["int64"], false));
        mem = allocate(*type, allocSize);
      } else {
        mem = builder.CreateBitCast(mem, (*type)->getPointerTo(0));
      }
      if (ast_->children_num > 5) {
        if (auto err = initialize(mem, ast_->children[5], *type)) {
          return err;
        }
      }
    } else {
      auto tp = types::Type{ast_->children[1]}.codegen(builder);
      if (!tp) {
        return tp.takeError();
      }
      const auto type = *tp;
      const auto allocSize = Integral::get(
          types::Type::getByteSize(module, type), BasicTypes["int64"]);
      mem = allocate(type, allocSize);
      if (ast_->children_num > 2) {
        if (auto err = initialize(mem, ast_->children[2], type)) {
          return err;
        }
      }
    }
    const auto newed = builder.CreateAlloca(mem->getType(), 0, nullptr, "");
    setIsDereferenceable(builder.getContext(), newed);
    builder.CreateStore(mem, newed);
    return newed;
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kNewExpr;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_NEWEXPR_HPP
