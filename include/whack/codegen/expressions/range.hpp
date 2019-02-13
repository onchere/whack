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
#ifndef WHACK_RANGE_HPP
#define WHACK_RANGE_HPP

#include "ast.hpp"

namespace whack::ast {

class Range final : public AST {
public:
  explicit Range(const mpc_ast_t* const ast)
      : begin_{getFactor(ast->children[0])} {
    if (ast->children_num > 4) {
      if (getOutermostAstTag(ast->children[2]) == "rangeable") {
        next_ = getFactor(ast->children[2]);
      }
    }
    const auto idx = ast->children_num - 1;
    if (getOutermostAstTag(ast->children[idx]) == "rangeable") {
      end_ = getFactor(ast->children[idx]);
    }
    if (std::string_view(ast->children[idx - 1]->contents) == "=") {
      endInclusive_ = true;
    }
  }

  inline auto begin(llvm::IRBuilder<>& builder) const {
    return begin_->codegen(builder);
  }

  inline const bool hasNext() const { return next_ != nullptr; }

  inline auto next(llvm::IRBuilder<>& builder) const {
    return next_ ? next_->codegen(builder) : error("no next"); //
  }

  inline auto end(llvm::IRBuilder<>& builder) const {
    return end_ ? end_->codegen(builder) : error("no end"); //
  }

  inline const bool endInclusive() const { return endInclusive_; }

private:
  std::unique_ptr<Factor> begin_;
  std::unique_ptr<Factor> next_;
  std::unique_ptr<Factor> end_;
  bool endInclusive_{false};
};

} // end namespace whack::ast

#endif // WHACK_RANGE_HPP
