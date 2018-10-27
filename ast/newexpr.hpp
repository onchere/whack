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
#ifndef WHACK_NEWEXPR_HPP
#define WHACK_NEWEXPR_HPP

#pragma once

#include "ast.hpp"
#include "metadata.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::ast {

class NewExpr final : public Factor {
public:
  explicit constexpr NewExpr(const mpc_ast_t* const ast) noexcept
      : Factor(kNewExpr), ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto block = builder.GetInsertBlock();
    const auto module = block->getParent()->getParent();
    // we cast the provided memory
    if (ast_->children_num > 2) {
      // @todo Check if memory is "enough"??
      auto expr = getExpressionValue(ast_->children[2])->codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      const auto mem = *expr;
      const auto type = Type{ast_->children[4]}.codegen(module);
      return llvm::dyn_cast<llvm::Value>(builder.CreateBitCast(mem, type));
    }
    const auto type = Type{ast_->children[1]}.codegen(module);
    const auto allocSize =
        type->isArrayTy() ? Integral{ast_->children[1]->children[1]}.value()
                          : 1;
    const auto call = llvm::CallInst::CreateMalloc(
        block, BasicTypes["int"], type,
        llvm::dyn_cast<llvm::Value>(
            llvm::ConstantInt::get(BasicTypes["int"], allocSize)),
        nullptr, nullptr, "");
    builder.Insert(call);
    return llvm::dyn_cast<llvm::Value>(call);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kNewExpr;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_NEWEXPR_HPP
