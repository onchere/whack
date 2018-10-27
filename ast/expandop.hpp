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
#ifndef WHACK_EXPANDOP_HPP
#define WHACK_EXPANDOP_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

// Implements parameter list expansion e.g. fun(params...)
class ExpandOp final : public Factor {
public:
  explicit ExpandOp(const mpc_ast_t* const ast)
      : Factor(kExpandOp), state_{ast->state}, variable_{getFactor(
                                                   ast->children[0])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto var = variable_->codegen(builder);
    if (!var) {
      return var.takeError();
    }
    const auto variable = *var;
    // const auto elementType = Type::getElementType(variable->getType());
    // const auto list = Type::getVariableLenArray(elementType);
    // return list;
    return error("not implemented");
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kExpandOp;
  }

private:
  const mpc_state_t state_;
  std::unique_ptr<Factor> variable_;
};

} // end namespace whack::ast

#endif // WHACK_EXPANDOP_HPP
