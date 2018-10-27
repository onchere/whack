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
#ifndef WHACK_COMPARISON_HPP
#define WHACK_COMPARISON_HPP

#pragma once

#include "ast.hpp"
#include "lexp.hpp"
#include <llvm/ADT/StringMap.h>

namespace whack::ast {

inline static llvm::StringMap<llvm::ICmpInst::Predicate> INTCMP{
    {">", llvm::ICmpInst::ICMP_SGT},  {"<", llvm::ICmpInst::ICMP_SLT},
    {">=", llvm::ICmpInst::ICMP_SGE}, {"==", llvm::ICmpInst::ICMP_EQ},
    {"!=", llvm::ICmpInst::ICMP_NE},  {"<=", llvm::ICmpInst::ICMP_SLE}};

inline static llvm::StringMap<llvm::FCmpInst::Predicate> REALCMP{
    {">", llvm::FCmpInst::FCMP_OGT},  {"<", llvm::FCmpInst::FCMP_OLT},
    {">=", llvm::FCmpInst::FCMP_OGE}, {"==", llvm::FCmpInst::FCMP_OEQ},
    {"!=", llvm::FCmpInst::FCMP_ONE}, {"<=", llvm::FCmpInst::FCMP_OLE}};

class Comparison final : public Expression {
public:
  explicit constexpr Comparison(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  // @todo Operator overloads for structs
  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    const auto tag = getOutermostAstTag(ast_->children[0]);
    if (tag == "lexp") {
      auto v = Lexp{ast_->children[0]}.codegen(builder);
      if (!v) {
        return v.takeError();
      }
      auto value = *v;
      for (auto i = 1; i < ast_->children_num; i += 2) {
        auto rhs = Lexp{ast_->children[i + 1]}.codegen(builder);
        if (!rhs) {
          return rhs.takeError();
        }
        const auto& op = ast_->children[i]->contents;
        const auto type = value->getType();
        if (type->isFloatingPointTy()) { //
          value = builder.CreateFCmp(REALCMP[op], value, *rhs);
        } else if (type->isIntegerTy()) {
          value = builder.CreateICmp(INTCMP[op], value, *rhs);
        } else {
          llvm_unreachable("TODO");
        }
      }
      return value;
    } else {
      if (std::string_view(ast_->children[0]->contents) == "!") {
        auto cmp = Comparison{ast_->children[2]}.codegen(builder);
        if (!cmp) {
          return cmp.takeError();
        }
        return builder.CreateNot(*cmp);
      } else {
        return Comparison{ast_->children[1]}.codegen(builder);
      }
    }
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_COMPARISON_HPP
