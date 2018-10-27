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
#ifndef WHACK_FORINEXPR_HPP
#define WHACK_FORINEXPR_HPP

#pragma once

#include "ast.hpp"
#include "condition.hpp"
#include "range.hpp"

namespace whack::ast {

class ForInExpr final : public AST {
public:
  explicit ForInExpr(const mpc_ast_t* const ast)
      : identList_{getIdentList(ast->children[1])}, range_{ast->children[3]} {
    if (ast->children_num > 4) {
      condition_ = std::make_unique<Condition>(ast->children[5]);
    }
  }

  inline const auto& identList() const { return identList_; }
  inline const auto& range() const { return range_; }
  inline const auto& condition() const { return condition_; }

private:
  ident_list_t identList_;
  Range range_;
  std::unique_ptr<Condition> condition_;
};

} // end namespace whack::ast

#endif // WHACK_FORINEXPR_HPP
