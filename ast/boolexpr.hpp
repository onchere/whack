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
#ifndef WHACK_BOOLEXPR_HPP
#define WHACK_BOOLEXPR_HPP

#pragma once

#include "ast.hpp"
#include "comparison.hpp"

namespace whack::ast {

class BoolExpr final : public Expression {
public:
  explicit BoolExpr(const mpc_ast_t* const ast) noexcept : ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    if (!ast_->children_num) {
      return Lexp{ast_}.codegen(builder);
    }
    const std::string_view sv{ast_->children[0]->contents};
    if (sv == "!") {
      auto expr = BoolExpr{ast_->children[2]}.codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      return builder.CreateNot(*expr);
    } else if (sv == "(") {
      return BoolExpr{ast_->children[1]}.codegen(builder);
    }
    if (getInnermostAstTag(ast_) == "comparison") {
      return Comparison{ast_}.codegen(builder);
    }
    const auto tag = getInnermostAstTag(ast_->children[0]);
    if (tag == "comparison") {
      auto val = Comparison{ast_->children[0]}.codegen(builder);
      if (!val) {
        return val.takeError();
      }
      auto value = *val;
      for (auto i = 1; i < ast_->children_num; i += 2) {
        const auto t = getOutermostAstTag(ast_->children[i + 1]);
        llvm::Value* rhs;
        if (t == "comparison") {
          auto expr = Comparison{ast_->children[i + 1]}.codegen(builder);
          if (!expr) {
            return expr.takeError();
          }
          rhs = *expr;
        } else { // "lexp"
          auto expr = Lexp{ast_->children[i + 1]}.codegen(builder);
          if (!expr) {
            return expr.takeError();
          }
          rhs = *expr;
        }
        if (std::string_view(ast_->children[i]->contents) == "&&") {
          value = builder.CreateAnd(value, rhs);
        } else {
          value = builder.CreateOr(value, rhs);
        }
      }
      return value;
    } else { // tag == "lexp"
      auto val = Lexp{ast_->children[0]}.codegen(builder);
      if (!val) {
        return val.takeError();
      }
      auto value = *val;
      for (auto i = 1; i < ast_->children_num; i += 2) {
        auto rhs = BoolExpr{ast_->children[i + 1]}.codegen(builder);
        if (!rhs) {
          return rhs.takeError();
        }
        if (std::string_view(ast_->children[i]->contents) == "&&") {
          value = builder.CreateAnd(value, *rhs);
        } else {
          value = builder.CreateOr(value, *rhs);
        }
      }
      return value;
    }
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_BOOLEXPR_HPP
