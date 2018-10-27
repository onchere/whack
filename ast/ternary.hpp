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
#ifndef WHACK_TERNARY_HPP
#define WHACK_TERNARY_HPP

#pragma once

#include "ast.hpp"
#include "condition.hpp"

namespace whack::ast {

class Ternary final : public Expression {
public:
  explicit Ternary(const mpc_ast_t* const ast)
      : state_{ast->state}, condition_{ast->children[0]},
        hence_{getExpressionValue(ast->children[2])},
        otherwise_{getExpressionValue(ast->children[4])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto cond = condition_.codegen(builder);
    if (!cond) {
      return cond.takeError();
    }
    auto hence = hence_->codegen(builder);
    if (!hence) {
      return hence.takeError();
    }
    auto otherwise = otherwise_->codegen(builder);
    if (!otherwise) {
      return otherwise.takeError();
    }
    if ((*hence)->getType() != (*otherwise)->getType()) {
      return error("type mismatch: expected expressions of the same type "
                   "for ternary expression at line {}",
                   state_.row + 1);
    }
    return builder.CreateSelect(*cond, *hence, *otherwise);
  }

private:
  const mpc_state_t state_;
  const Condition condition_;
  std::unique_ptr<Expression> hence_, otherwise_;
};
} // namespace whack::ast

#endif // WHACK_TERNARY_HPP
