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
#ifndef WHACK_EXPANSION_HPP
#define WHACK_EXPANSION_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Expansion final : public Factor {
public:
  explicit constexpr Expansion(const mpc_ast_t* const ast)
      : Factor(kExpansion), state_{ast->state} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    const auto placeholder =
        module->getOrInsertGlobal("::expansion", BasicTypes["bool"]);
    return llvm::cast<llvm::Value>(placeholder);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kExpansion;
  }

private:
  const mpc_state_t state_;
};

} // end namespace whack::ast

#endif // WHACK_EXPANSION_HPP
