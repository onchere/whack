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
#ifndef WHACK_XOR_HPP
#define WHACK_XOR_HPP

#pragma once

#include "bitwise_and.hpp"

namespace whack::codegen::expressions::operators {

class Xor final : public AST {
  using bitwise_and_t = std::unique_ptr<BitwiseAnd>;

public:
  explicit Xor(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "bitwisexor") {
      initial_ = std::make_unique<BitwiseAnd>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_.emplace_back(
            std::make_unique<BitwiseAnd>(ast->children[i + 1]));
      }
    } else {
      initial_ = std::make_unique<BitwiseAnd>(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    auto init = initial_->codegen(builder);
    if (!init) {
      return init.takeError();
    }
    auto ret = *init;
    for (size_t i = 0; i < others_.size(); ++i) {
      auto next = others_[i]->codegen(builder);
      if (!next) {
        return next.takeError();
      }
      auto l = getLoadedValue(builder, ret);
      if (!l) {
        return l.takeError();
      }
      auto r = getLoadedValue(builder, *next);
      if (!r) {
        return r.takeError();
      }
      auto xoR = get(builder, *l, *r);
      if (!xoR) {
        return xoR.takeError();
      }
      ret = *xoR;
    }
    return ret;
  }

  static llvm::Expected<llvm::Value*> get(llvm::IRBuilder<>& builder,
                                          llvm::Value* const lhs,
                                          llvm::Value* const rhs) {
    const auto lhsType = lhs->getType();
    if (lhsType != rhs->getType()) {
      return error("type mismatch in operator^");
    }
    if (lhsType->isIntegerTy() || lhsType->isFloatingPointTy()) {
      return builder.CreateXor(lhs, rhs);
    }
    if (lhsType->isStructTy()) {
      if (auto apply = applyStructOperator(builder, lhs, "^", rhs)) {
        return apply.value();
      }
    }
    return error("operator^ not implemented for type");
  }

private:
  const mpc_state_t state_;
  bitwise_and_t initial_;
  small_vector<bitwise_and_t> others_;
};

} // namespace whack::codegen::expressions::operators

#endif // WHACK_XOR_HPP
