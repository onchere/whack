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
#ifndef WHACK_INITIALIZER_HPP
#define WHACK_INITIALIZER_HPP

#pragma once

#include "initlist.hpp"
#include "memberinitlist.hpp"

namespace whack::codegen::expressions::factors {

class Initializer final : public AST {
public:
  using list_t = std::variant<InitList, MemberInitList>;

  explicit constexpr Initializer(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder,
                                       llvm::Type* const type) const {
    const auto init = list();
    if (init.index() == 0) {
      return std::get<0>(init).codegen(builder, type);
    }
    return std::get<1>(init).codegen(builder, type);
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    const auto init = list();
    if (init.index() == 0) {
      return std::get<0>(init).codegen(builder);
    }
    return std::get<1>(init).codegen(builder);
  }

  const list_t list() const {
    if (getInnermostAstTag(ast_) == "initlist") {
      return InitList{ast_};
    }
    return MemberInitList{ast_->children[1]};
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_INITIALIZER_HPP
