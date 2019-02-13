/**
 * Copyright 2019-present Onchere Bironga
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
#ifndef WHACK_LOGICALAND_HPP
#define WHACK_LOGICALAND_HPP

#pragma once

#include "bitwise_or.hpp"

namespace whack::codegen::expressions::operators {

class LogicalAnd final : public AST {
  using bitwise_or_t = std::unique_ptr<BitwiseOr>;

public:
  explicit LogicalAnd(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "logicaland") {
      initial_ = std::make_unique<BitwiseOr>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_.emplace_back(std::make_unique<BitwiseOr>(ast->children[i + 1]));
      }
    } else {
      initial_ = std::make_unique<BitwiseOr>(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    auto init = initial_->codegen(builder);
    if (!init) {
      return init.takeError();
    }
    auto ret = *init;
    if (others_.empty()) {
      return ret;
    }
    if (!ret->getType()->isIntegerTy(1)) {
      return error("logical-and operator not implemented for type at line {}",
                   state_.row + 1);
    }
    for (size_t i = 0; i < others_.size(); ++i) {
      auto next = others_[i]->codegen(builder);
      if (!next) {
        return next.takeError();
      }
      if (!(*next)->getType()->isIntegerTy(1)) {
        return error("type mismatch in logical-and operator at line {}",
                     state_.row + 1);
      }
      auto l = getLoadedValue(builder, ret);
      if (!l) {
        return l.takeError();
      }
      auto r = getLoadedValue(builder, *next);
      if (!r) {
        return r.takeError();
      }
      ret = builder.CreateAnd(*l, *r);
    }
    return ret;
  }

private:
  const mpc_state_t state_;
  bitwise_or_t initial_;
  small_vector<bitwise_or_t> others_;
};

} // end namespace whack::codegen::expressions::operators

#endif // WHACK_LOGICALAND_HPP
