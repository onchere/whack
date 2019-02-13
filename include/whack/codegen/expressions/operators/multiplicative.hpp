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
#ifndef WHACK_MULTIPLICATIVE_HPP
#define WHACK_MULTIPLICATIVE_HPP

#pragma once

#include "../factors/factor.hpp"
#include <llvm/ADT/StringMap.h>

namespace whack::codegen::expressions::operators {

// Type checking is up to the caller @todo
static std::optional<llvm::Value*>
applyStructOperator(llvm::IRBuilder<>& builder, llvm::Value* const lhs,
                    const llvm::StringRef op, llvm::Value* const rhs) {
  const auto funcName =
      format("struct::{}::operator {}", lhs->getType()->getStructName().data(),
             op.data());
  const auto module = builder.GetInsertBlock()->getModule();
  if (const auto func = module->getFunction(funcName)) {
    return builder.CreateCall(func, {lhs, rhs});
  }
  return std::nullopt;
}

class Multiplicative final : public AST {
public:
  explicit Multiplicative(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "multiplicative") {
      initial_ = factors::getFactor(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_[ast->children[i]->contents] =
            factors::getFactor(ast->children[i + 1]);
      }
    } else {
      initial_ = factors::getFactor(ast);
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
      auto mul = get(builder, *l, other.getKey(), *r);
      if (!mul) {
        return mul.takeError();
      }
      ret = *mul;
    }
    return ret;
  }

  static llvm::Expected<llvm::Value*> get(llvm::IRBuilder<>& builder,
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
      if (op == "/") {
        return builder.CreateSDiv(lhs, rhs);
      }
      if (op == "*") {
        return builder.CreateMul(lhs, rhs);
      }
      return builder.CreateSRem(lhs, rhs);
    }
    if (lhsType->isFloatingPointTy()) {
      if (op == "/") {
        return builder.CreateFDiv(lhs, rhs);
      }
      if (op == "*") {
        return builder.CreateFMul(lhs, rhs);
      }
      return builder.CreateFRem(lhs, rhs);
    }
    return error("operator{} not implemented for type", op.data());
  }

private:
  const mpc_state_t state_;
  using factor_t = std::unique_ptr<factors::Factor>;
  factor_t initial_;
  llvm::StringMap<factor_t> others_;
};

} // end namespace whack::codegen::expressions::operators

#endif // WHACK_MULTIPLICATIVE_HPP
