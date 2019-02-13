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
#ifndef WHACK_EQUALITY_HPP
#define WHACK_EQUALITY_HPP

#pragma once

#include "relational.hpp"

namespace whack::codegen::expressions::operators {

class Equality final : public AST {
  using relational_t = std::unique_ptr<Relational>;

public:
  explicit Equality(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "equality") {
      initial_ = std::make_unique<Relational>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_[ast->children[i]->contents] =
            std::make_unique<Relational>(ast->children[i + 1]);
      }
    } else {
      initial_ = std::make_unique<Relational>(ast);
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

  inline static llvm::Expected<llvm::Value*> get(llvm::IRBuilder<>& builder,
                                                 llvm::Value* const lhs,
                                                 const llvm::StringRef op,
                                                 llvm::Value* const rhs) {
    return Relational::get(builder, lhs, op, rhs);
  }

private:
  const mpc_state_t state_;
  relational_t initial_;
  llvm::StringMap<relational_t> others_;
};

// @todo
inline static llvm::Expected<llvm::Value*>
getEquality(llvm::IRBuilder<>& builder, llvm::Value* const lhs,
            const llvm::StringRef op, llvm::Value* const rhs) {
  return Equality::get(builder, lhs, op, rhs);
}

} // namespace whack::codegen::expressions::operators

#endif // WHACK_EQUALITY_HPP
