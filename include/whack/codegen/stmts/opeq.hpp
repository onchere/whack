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
#ifndef WHACK_OPEQ_HPP
#define WHACK_OPEQ_HPP

#pragma once

namespace whack::codegen::stmts {

class OpEq final : public Stmt {
public:
  explicit OpEq(const mpc_ast_t* const ast)
      : Stmt(kOpEq), state_{ast->state},
        variable_{expressions::factors::getFactor(ast->children[0])},
        op_{ast->children[1]->contents}, expr_{expressions::getExpressionValue(
                                             ast->children[3])} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto var = variable_->codegen(builder);
    if (!var) {
      return var.takeError();
    }
    auto e = expr_->codegen(builder);
    if (!e) {
      return e.takeError();
    }
    auto variable = *var;
    auto expression = expressions::getLoadedValue(builder, *e);
    if (!expression) {
      return expression.takeError();
    }
    const auto expr = *expression;
    using namespace expressions::operators;
    auto res = [&]() -> llvm::Expected<llvm::Value*> {
      auto l = expressions::getLoadedValue(builder, variable);
      if (!l) {
        return l.takeError();
      }
      const auto lhs = *l;
      if (op_ == "+" || op_ == "-") {
        return getAdditive(builder, lhs, op_, expr);
      }
      if (op_ == "/" || op_ == "*" || op_ == "%") {
        return Multiplicative::get(builder, lhs, op_, expr);
      }
      if (op_ == "<<" || op_ == ">>") {
        return Shift::get(builder, lhs, op_, expr);
      }
      if (op_ == "^") {
        return Xor::get(builder, lhs, expr);
      }
      if (op_ == "|") {
        return BitwiseOr::get(builder, lhs, expr);
      }
      // op_ == "&"
      return BitwiseAnd::get(builder, lhs, expr);
    }();
    if (!res) {
      return res.takeError();
    }
    if (hasMetadata(variable, llvm::LLVMContext::MD_dereferenceable)) {
      variable = builder.CreateLoad(variable);
    }
    builder.CreateStore(*res, variable);
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kOpEq;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<expressions::factors::Factor> variable_;
  const llvm::StringRef op_;
  const std::unique_ptr<Expression> expr_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_OPEQ_HPP
