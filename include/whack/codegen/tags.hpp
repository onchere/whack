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
#ifndef WHACK_TAGS_HPP
#define WHACK_TAGS_HPP

#pragma once

#include "expressions/factors/ident.hpp"
#include "expressions/factors/scoperes.hpp"
#include <variant>

namespace whack::codegen {

class Tags final : public AST {
  using ScopeRes = expressions::factors::ScopeRes;
  using Ident = expressions::factors::Ident;

public:
  using tag_name_t = std::variant<ScopeRes, Ident>;
  using tag_t = std::pair<tag_name_t, std::optional<small_vector<expr_t>>>;

  explicit Tags(const mpc_ast_t* const ast) {
    if (ast->children_num == 2) { // single tag
      tags_.emplace_back(getTag(ast->children[1]));
    } else {
      for (auto i = 2; i < ast->children_num - 1; i += 2) {
        tags_.emplace_back(getTag(ast->children[i]));
      }
    }
  }

  inline const auto& get() const { return tags_; }

private:
  std::vector<tag_t> tags_;

  static tag_name_t getName(const mpc_ast_t* const ast) {
    return getInnermostAstTag(ast) == "scoperes"
               ? static_cast<tag_name_t>(ScopeRes{ast})
               : static_cast<tag_name_t>(Ident{ast});
  }

  static tag_t getTag(const mpc_ast_t* const ast) {
    if (ast->children_num) {
      return {getName(ast->children[0]),
              std::optional{expressions::getExprList(ast->children[2])}};
    }
    return {getName(ast), std::nullopt};
  }
};

} // end namespace whack::codegen

#endif // WHACK_TAGS_HPP
