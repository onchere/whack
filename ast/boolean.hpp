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
#ifndef WHACK_BOOLEAN_HPP
#define WHACK_BOOLEAN_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Boolean final : public Factor {
public:
  explicit constexpr Boolean(const mpc_ast_t* const ast)
      : Factor(kBoolean), boolean_{std::string_view(ast->contents) == "true"} {}

  inline llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>& builder) const final {
    return builder.getInt1(boolean_);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kBoolean;
  }

private:
  const bool boolean_;
};

} // end namespace whack::ast

#endif // WHACK_BOOLEAN_HPP
