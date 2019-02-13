/**
 * Copyright 2018-present Onchere Bironga
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
#ifndef WHACK_EXPRESSION_HPP
#define WHACK_EXPRESSION_HPP

#pragma once

#include "addrof.hpp"
#include "operators/logical_or.hpp"
#include "ternary.hpp"

namespace whack::codegen::expressions {

static expr_t getExpressionValue(const mpc_ast_t* const ast) {
  const auto tag = getInnermostAstTag(ast);
  if (tag == "addrof") {
    return std::make_unique<AddressOf>(ast);
  }
  if (tag == "ternary") {
    return std::make_unique<Ternary>(ast);
  }
  return std::make_unique<operators::LogicalOr>(ast);
}

static small_vector<expr_t> getExprList(const mpc_ast_t* const ast) {
  small_vector<expr_t> exprList;
  if (getInnermostAstTag(ast) == "exprlist") {
    for (auto i = 0; i < ast->children_num; i += 2) {
      exprList.emplace_back(getExpressionValue(ast->children[i]));
    }
  } else {
    exprList.emplace_back(getExpressionValue(ast));
  }
  return exprList;
}

static llvm::Expected<llvm::Value*>
getLoadedValue(llvm::IRBuilder<>& builder, llvm::Value* const value,
               const bool allowExpansion) {
  if (!allowExpansion && llvm::isa<factors::Expansion>(value)) {
    return error("cannot use expansion in this context");
  }
  static constexpr auto MetadataID = llvm::LLVMContext::MD_dereferenceable;
  auto ret = value;
  if (hasMetadata(ret, MetadataID)) {
    ret = builder.CreateLoad(ret);
  }
  if (llvm::isa<llvm::AllocaInst>(ret)) {
    ret = builder.CreateLoad(ret);
  } else if (llvm::isa<llvm::LoadInst>(ret)) {
    if (hasMetadata(llvm::cast<llvm::LoadInst>(ret)->getPointerOperand(),
                    MetadataID)) {
      ret = builder.CreateLoad(ret);
    }
  }
  return ret;
}

static llvm::Expected<small_vector<llvm::Value*>>
getExprValues(llvm::IRBuilder<>& builder, const small_vector<expr_t>& exprList,
              const bool allowExpansion) {
  small_vector<llvm::Value*> values;
  for (const auto& expr : exprList) {
    auto val = expr->codegen(builder);
    if (!val) {
      return val.takeError();
    }
    auto v = getLoadedValue(builder, *val, allowExpansion);
    if (!v) {
      return v.takeError();
    }
    values.push_back(*v);
  }
  return values;
}

} // end namespace whack::codegen::expressions

#endif // WHACK_EXPRESSION_HPP
