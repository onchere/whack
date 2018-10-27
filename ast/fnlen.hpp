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
#ifndef WHACK_FNLEN_HPP
#define WHACK_FNLEN_HPP

#include "ast.hpp"

#pragma once

namespace whack::ast {

class FnLen final : public Factor {
public:
  explicit FnLen(const mpc_ast_t* const ast)
      : expr_{getExpressionValue(ast->children[2])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto e = expr_->codegen(builder);
    if (!e) {
      return e.takeError();
    }
    const auto expr = *e;
    const auto type = expr->getType();
    if (Type::isVariableLengthArray(type)) {
      // @todo
    }
    return Integral::one(); // @todo
  }

private:
  std::unique_ptr<Expression> expr_;
};

} // end namespace whack::ast

#endif // WHACK_FNLEN_HPP
