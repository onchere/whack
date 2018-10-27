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
#ifndef WHACK_CONDITION_HPP
#define WHACK_CONDITION_HPP

#pragma once

#include "ast.hpp"
#include "conditional.hpp"

namespace whack::ast {

class Condition final : public AST {
public:
  explicit Condition(const mpc_ast_t* const ast) {
    if (getInnermostAstTag(ast) == "condition") {
      auto ref = ast;
      const std::string_view sv{ast->children[0]->contents};
      if (sv == "!") {
        ref = ast->children[2];
        negate_ = true;
      } else if (sv == "(") {
        ref = ast->children[1];
      }
      initial_ = std::make_unique<Conditional>(ref->children[0]);
      for (auto i = 1; i < ref->children_num; i += 2) {
        auto cond = std::make_pair(ref->children[i]->contents,
                                   Conditional{ref->children[i + 1]});
        others_.emplace_back(std::move(cond));
      }
    } else {
      initial_ = std::make_unique<Conditional>(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    auto val = initial_->codegen(builder);
    if (!val) {
      return val.takeError();
    }
    auto value = *val;
    for (const auto& [op, conditional] : others_) {
      auto rhs = conditional.codegen(builder);
      if (!rhs) {
        return rhs.takeError();
      }
      if (op == "&&") {
        value = builder.CreateAnd(value, *rhs);
      } else if (op == "||") {
        value = builder.CreateOr(value, *rhs);
      } else {
        llvm_unreachable("invalid op kind!");
      }
    }
    if (negate_) {
      return builder.CreateNot(value);
    }
    return value;
  }

private:
  bool negate_{false};
  std::unique_ptr<Conditional> initial_;
  small_vector<std::pair<std::string, Conditional>> others_;
};

} // end namespace whack::ast

#endif // WHACK_CONDITION_HPP
