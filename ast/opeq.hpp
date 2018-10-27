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
#ifndef WHACK_OPEQ_HPP
#define WHACK_OPEQ_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class OpEq final : public Stmt {
public:
  explicit OpEq(const mpc_ast_t* const ast)
      : Stmt(kOpEq), state_{ast->state}, variable_{getFactor(ast->children[0])},
        op_{ast->children[1]->contents}, expr_{getExpressionValue(
                                             ast->children[3])} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto var = variable_->codegen(builder);
    if (!var) {
      return var.takeError();
    }
    const auto variable = *var;
    const auto type = variable->getType()->getPointerElementType();
    auto op = op_;
    if (const auto [structType, isStruct] = Type::isStructKind(type);
        isStruct) {
      const auto module = builder.GetInsertBlock()->getModule();
      const auto structName = structType->getStructName().str();
      if (const auto func =
              module->getFunction(format("struct::{}::{}", structName, op))) {
        auto e = expr_->codegen(builder);
        if (!e) {
          return e.takeError();
        }
        const auto expr = *e;
        const auto value = builder.CreateCall(func, {variable, expr});
        builder.CreateStore(value, variable);
        return llvm::Error::success();
      } else {
        return error("cannot find operator`{}` for struct `{}` "
                     " at line {}",
                     op_, structName, state_.row + 1);
      }
    } else if (type->isIntegerTy() || type->isFloatingPointTy()) {
      if (type->isFloatingPointTy() &&
          (op == "+" || op == "-" || op == "*" || op == "/")) {
        op += "f";
      }
      if (OpsTable.find(op) == OpsTable.end()) {
        return error("invalid operator `{}` at line {}", op_, state_.row + 1);
      }
      auto e = expr_->codegen(builder);
      if (!e) {
        return e.takeError();
      }
      const auto expr = *e;
      const auto value = reinterpret_cast<llvm::Value*>(OpsTable[op](
          reinterpret_cast<LLVMBuilderRef>(&builder),
          reinterpret_cast<LLVMValueRef>(builder.CreateLoad(variable)),
          reinterpret_cast<LLVMValueRef>(expr), ""));
      builder.CreateStore(value, variable);
      return llvm::Error::success();
    }
    return error("invalid type for operator{} at line {}", op, state_.row + 1);
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kOpEq;
  }

private:
  const mpc_state_t state_;
  std::unique_ptr<Factor> variable_;
  const std::string op_;
  std::unique_ptr<Expression> expr_;
};

} // end namespace whack::ast

#endif // WHACK_OPEQ_HPP
