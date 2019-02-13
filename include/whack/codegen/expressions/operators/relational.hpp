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
#ifndef WHACK_RELATIONAL_HPP
#define WHACK_RELATIONAL_HPP

#pragma once

#include "shift.hpp"

namespace whack::codegen::expressions::operators {

inline static llvm::StringMap<llvm::ICmpInst::Predicate> IntCmp{
    {">", llvm::ICmpInst::ICMP_SGT},  {"<", llvm::ICmpInst::ICMP_SLT},
    {">=", llvm::ICmpInst::ICMP_SGE}, {"==", llvm::ICmpInst::ICMP_EQ},
    {"!=", llvm::ICmpInst::ICMP_NE},  {"<=", llvm::ICmpInst::ICMP_SLE}};

inline static llvm::StringMap<llvm::ICmpInst::Predicate> UIntCmp{
    {">", llvm::ICmpInst::ICMP_UGT},  {"<", llvm::ICmpInst::ICMP_ULT},
    {">=", llvm::ICmpInst::ICMP_UGE}, {"==", llvm::ICmpInst::ICMP_EQ},
    {"!=", llvm::ICmpInst::ICMP_NE},  {"<=", llvm::ICmpInst::ICMP_ULE}};

inline static llvm::StringMap<llvm::FCmpInst::Predicate> FCmp{
    {">", llvm::FCmpInst::FCMP_OGT},  {"<", llvm::FCmpInst::FCMP_OLT},
    {">=", llvm::FCmpInst::FCMP_OGE}, {"==", llvm::FCmpInst::FCMP_OEQ},
    {"!=", llvm::FCmpInst::FCMP_ONE}, {"<=", llvm::FCmpInst::FCMP_OLE}};

class Relational final : public AST {
  using shift_t = std::unique_ptr<Shift>;

public:
  explicit Relational(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "relational") {
      initial_ = std::make_unique<Shift>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_[ast->children[i]->contents] =
            std::make_unique<Shift>(ast->children[i + 1]);
      }
    } else {
      initial_ = std::make_unique<Shift>(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    auto init = initial_->codegen(builder);
    if (!init) {
      return init.takeError();
    }
    auto agg = *init;
    llvm::Value* prev = nullptr;
    for (const auto& other : others_) {
      auto rhs = other.getValue()->codegen(builder);
      if (!rhs) {
        return rhs.takeError();
      }
      if (prev == nullptr) {
        auto l = getLoadedValue(builder, agg);
        if (!l) {
          return l.takeError();
        }
        auto r = getLoadedValue(builder, *rhs);
        if (!r) {
          return r.takeError();
        }
        auto cmp = get(builder, *l, other.getKey(), *r);
        if (!cmp) {
          return cmp.takeError();
        }
        agg = *cmp;
      } else {
        auto l = getLoadedValue(builder, prev);
        if (!l) {
          return l.takeError();
        }
        auto r = getLoadedValue(builder, *rhs);
        if (!r) {
          return r.takeError();
        }
        auto cmp = get(builder, *l, other.getKey(), *r);
        if (!cmp) {
          return cmp.takeError();
        }
        agg = builder.CreateAnd(agg, *cmp);
      }
      prev = agg;
    }
    return agg;
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
      return builder.CreateICmp(IntCmp[op], lhs, rhs);
    }
    if (lhsType->isFloatingPointTy()) {
      return builder.CreateFCmp(FCmp[op], lhs, rhs);
    }
    if (lhsType->isStructTy()) {
      if (auto apply = applyStructOperator(builder, lhs, op, rhs)) {
        return apply.value();
      }
    }
    // @todo strcmp for char*'s if == or !=?
    // @todo owise Memcmp?
    return error("operator{} not implemented for type", op.data());
  }

private:
  const mpc_state_t state_;
  shift_t initial_;
  llvm::StringMap<shift_t> others_;
};

} // namespace whack::codegen::expressions::operators

#endif // WHACK_RELATIONAL_HPP
