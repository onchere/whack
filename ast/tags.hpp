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
#ifndef WHACK_TAGS_HPP
#define WHACK_TAGS_HPP

#pragma once

#include "ast.hpp"
#include "ident.hpp"
#include "scoperes.hpp"

namespace whack::ast {

class Tags final : public AST {
public:
  // using tag_name_t = std::variant<ScopeRes, Ident>;
  // using tag_t = std::pair<tag_name_t, small_vector<expr_t>>;

  explicit Tags(const mpc_ast_t* const ast) {
    mpc_ast_print((mpc_ast_t*)ast);
    for (auto i = 2; i < ast->children_num - 1; i += 2) {
      const auto ref = ast->children[i];
      // auto name = getInnermostAstTag(ref) == "scoperes"
      //                 ? static_cast<tag_name_t>(ScopeRes{ref})
      //                 : static_cast<tag_name_t>(Ident{ref});
      // small_vector<expr_t> exprList;
      // if (std::string_view(ast->children[i + 1]->contents) == "(") {
      //   exprList = getExprList(ast->children[i + 2]);
      //   i += 3;
      // }
      // tags_.emplace_back(std::pair{std::move(name), std::move(exprList)});
    }
  }

  // inline const auto& get() const { return tags_; }

private:
  // small_vector<tag_t> tags_;
};

} // end namespace whack::ast

#endif // WHACK_TAGS_HPP
