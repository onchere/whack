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
#ifndef WHACK_EXPANSION_HPP
#define WHACK_EXPANSION_HPP

#pragma once

#include <iostream>

namespace whack::codegen::expressions::factors {

class Expansion final : public Factor {
public:
  constexpr Expansion(const mpc_ast_t* const = nullptr) noexcept
      : Factor(kExpansion) {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    const auto placeholder =
        module->getOrInsertGlobal("::expansion", BasicTypes["bool"]);
    return llvm::cast<llvm::Value>(placeholder);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kExpansion;
  }

  inline static bool classof(const llvm::Value* const value) {
    return value->getName() == "::expansion";
  }
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_EXPANSION_HPP
