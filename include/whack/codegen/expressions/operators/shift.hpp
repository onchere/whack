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
#ifndef WHACK_SHIFT_HPP
#define WHACK_SHIFT_HPP

#pragma once

#include "additive.hpp"

namespace whack::codegen::expressions::operators {

class Shift final : public AST {
  using additive_t = std::unique_ptr<Additive>;

public:
  explicit Shift(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "shift") {
      initial_ = std::make_unique<Additive>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_[ast->children[i]->contents] =
            std::make_unique<Additive>(ast->children[i + 1]);
      }
    } else {
      initial_ = std::make_unique<Additive>(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    auto init = initial_->codegen(builder);
    if (!init) {
      return init.takeError();
    }
    auto ret = *init;
    for (const auto& other : others_) {
      auto rhs = other.getValue()->codegen(builder);
      if (!rhs) {
        return rhs.takeError();
      }
      auto l = getLoadedValue(builder, ret);
      if (!l) {
        return l.takeError();
      }
      auto r = getLoadedValue(builder, *rhs);
      if (!r) {
        return r.takeError();
      }
      auto shift = get(builder, *l, other.getKey(), *r);
      if (!shift) {
        return shift.takeError();
      }
      ret = *shift;
    }
    return ret;
  }

  static llvm::Expected<llvm::Value*> get(llvm::IRBuilder<>& builder,
                                          llvm::Value* const lhs,
                                          const llvm::StringRef op,
                                          llvm::Value* const rhs) {
    const auto lhsType = lhs->getType();
    if (lhsType != rhs->getType()) {
      return error("type mismatch in operator{}", op.data());
    }
    if (lhsType->isIntegerTy()) {
      if (op == "<<") {
        return builder.CreateShl(lhs, rhs);
      }
      return builder.CreateAShr(lhs, rhs);
    }
    if (lhsType->isStructTy()) {
      if (auto apply = applyStructOperator(builder, lhs, op, rhs)) {
        return apply.value();
      }
    }
    return error("operator{} not implemented for type", op.data());
  }

private:
  const mpc_state_t state_;
  additive_t initial_;
  llvm::StringMap<additive_t> others_;
};

} // namespace whack::codegen::expressions::operators

#endif // WHACK_SHIFT_HPP
