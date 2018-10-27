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
#ifndef WHACK_FNCAST_HPP
#define WHACK_FNCAST_HPP

#pragma once

#include "ast.hpp"
#include "type.hpp"

namespace whack::ast {

class FnCast final : public Factor {
public:
  explicit FnCast(const mpc_ast_t* const ast)
      : state_{ast->state}, typeTo_{ast->children[2]}, expr_{getExpressionValue(
                                                           ast->children[5])}

  {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto e = expr_->codegen(builder);
    if (!e) {
      return e.takeError();
    }
    auto expr = *e;
    const auto typeFrom = expr->getType();
    const auto module = builder.GetInsertBlock()->getModule();
    const auto typeTo = typeTo_.codegen(module);
    if (typeFrom->isIntegerTy()) {
      if (typeTo->isFloatingPointTy()) {
        return builder.CreateSIToFP(expr, typeTo);
      }
      return builder.CreateZExtOrTrunc(expr, typeTo);
    } else if (typeFrom->isFloatingPointTy()) {
      if (typeTo->isIntegerTy()) {
        return builder.CreateFPToSI(expr, typeTo);
      } else if (typeFrom->isDoubleTy() && typeTo->isFloatTy()) {
        return builder.CreateFPTrunc(expr, typeTo);
      } else if (typeFrom->isFloatTy() && typeTo->isDoubleTy()) {
        return builder.CreateFPExt(expr, typeTo);
      }
      return expr;
    } else if (typeFrom->isPointerTy()) {
      // @todo: Overloaded operators...
      llvm_unreachable("TODO");
    } else if (typeTo->isPointerTy()) {
      // @todo
      llvm_unreachable("TODO");
    }
    return error("invalid cast at line {}", state_.row + 1);
  }

private:
  const mpc_state_t state_;
  Type typeTo_;
  std::unique_ptr<Expression> expr_;
};

} // end namespace whack::ast

#endif // WHACK_FNCAST_HPP
