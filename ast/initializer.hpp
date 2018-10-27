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
#ifndef WHACK_INITIALIZER_HPP
#define WHACK_INITIALIZER_HPP

#pragma once

#include "ast.hpp"
#include "initlist.hpp"
#include "listcomprehension.hpp"
#include "memberinitlist.hpp"
#include <variant>

namespace whack::ast {

class Initializer final : public AST {
public:
  explicit constexpr Initializer(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  using list_t = std::variant<InitList, ListComprehension, MemberInitList>;

  const list_t list() const {
    const auto tag = getInnermostAstTag(ast_);
    if (tag == "memberinitlist") {
      return MemberInitList{ast_};
    }
    if (tag == "listcomprehension") {
      return ListComprehension{ast_};
    }
    return InitList{ast_};
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_INITIALIZER_HPP
