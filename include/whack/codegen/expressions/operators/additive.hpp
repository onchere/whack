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
#ifndef WHACK_ADDITIVE_HPP
#define WHACK_ADDITIVE_HPP

#pragma once

#include "multiplicative.hpp"

namespace whack::codegen::expressions::operators {

class Additive final : public AST {
  using multiplicative_t = std::unique_ptr<Multiplicative>;

public:
  explicit Additive(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "additive") {
      initial_ = std::make_unique<Multiplicative>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_[ast->children[i]->contents] =
            std::make_unique<Multiplicative>(ast->children[i + 1]);
      }
    } else {
      initial_ = std::make_unique<Multiplicative>(ast);
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
      auto add = getAdditive(builder, *l, other.getKey(), *r);
      if (!add) {
        return add.takeError();
      }
      ret = *add;
    }
    return ret;
  }

private:
  const mpc_state_t state_;
  multiplicative_t initial_;
  llvm::StringMap<multiplicative_t> others_;
};

static llvm::Expected<llvm::Value*> getAdditive(llvm::IRBuilder<>& builder,
                                                llvm::Value* const lhs,
                                                const llvm::StringRef op,
                                                llvm::Value* const rhs) {
  const auto lhsType = lhs->getType();
  if (lhsType->isStructTy()) {
    if (auto apply = applyStructOperator(builder, lhs, op, rhs)) {
      return apply.value();
    }
  }
  if (lhsType != rhs->getType()) {
    return error("type mismatch in operator{}", op.data());
  }
  // @todo Signed/Unsigned operations
  // For now, we assume all integers are signed
  if (lhsType->isIntegerTy()) {
    if (op == "+") {
      return builder.CreateAdd(lhs, rhs);
    }
    return builder.CreateSub(lhs, rhs);
  }
  if (lhsType->isFloatingPointTy()) {
    if (op == "+") {
      return builder.CreateFAdd(lhs, rhs);
    }
    return builder.CreateFSub(lhs, rhs);
  }
  if (lhsType->isPointerTy() && op == "-") {
    return builder.CreatePtrDiff(lhs, rhs);
  }
  return error("operator{} not implemented for type", op.data());
}

} // namespace whack::codegen::expressions::operators

#endif // WHACK_ADDITIVE_HPP
