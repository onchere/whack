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
#ifndef WHACK_EXPRESSION_HPP
#define WHACK_EXPRESSION_HPP

#pragma once

#include "addrof.hpp"
#include "ast.hpp"
#include "boolexpr.hpp"
#include "lexp.hpp"
#include "ternary.hpp"

namespace whack::ast {

static expr_t getExpressionValue(const mpc_ast_t* const ast) {
  const auto tag = getInnermostAstTag(ast);
  if (tag == "addrof") {
    return std::make_unique<AddrOf>(ast);
  } else if (tag == "ternary") {
    return std::make_unique<Ternary>(ast);
  } else if (tag == "boolexpr") {
    return std::make_unique<BoolExpr>(ast);
  } else if (tag == "comparison") { // <boolexpr>
    return std::make_unique<Comparison>(ast);
  }
  return std::make_unique<Lexp>(ast);
}

static small_vector<expr_t> getExprList(const mpc_ast_t* const ast) {
  small_vector<expr_t> exprList;
  if (getInnermostAstTag(ast) == "exprlist") {
    for (auto i = 0; i < ast->children_num; i += 2) {
      exprList.emplace_back(getExpressionValue(ast->children[i]));
    }
  } else {
    exprList.emplace_back(getExpressionValue(ast));
  }
  return exprList;
}

} // end namespace whack::ast

#endif // WHACK_EXPRESSION_HPP
