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
#ifndef WHACK_STRUCTMEMBER_HPP
#define WHACK_STRUCTMEMBER_HPP

#pragma once

#include "ast.hpp"
#include "metadata.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

class StructMember final : public Factor {
public:
  explicit constexpr StructMember(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto func = builder.GetInsertBlock()->getParent();
    const auto symTable = func->getValueSymbolTable();
    auto extracted = symTable->lookup(ast_->children[0]->contents);
    if (!extracted) {
      return error("variable `{}` does not exist in scope "
                   "at line {}",
                   ast_->children[0]->contents, ast_->state.row + 1);
    }

    const auto& module = *func->getParent();
    for (auto i = 2; i < ast_->children_num; i += 2) {
      auto type = extracted->getType();
      if (!type->isStructTy()) {
        type = type->getPointerElementType();
        if (!type->isStructTy()) {
          return error("expected `{}` to be a struct type at "
                       "line {}",
                       ast_->children[i - 2]->contents, ast_->state.row + 1);
        }
      }

      const auto structName = type->getStructName().str();
      const auto memberRef = ast_->children[i];
      const auto& member = memberRef->contents;
      if (const auto idx = getIndex(module, structName, member)) {
        extracted =
            builder.CreateStructGEP(type, extracted, idx.value(), member);
      } else if (const auto memFun = module.getFunction(
                     format("struct::{}::{}", structName, member))) {
        // assert(ast_->children_num == 3);
        const auto newFuncType = llvm::FunctionType::get(
            memFun->getReturnType(),
            memFun->getFunctionType()->params().drop_front(),
            memFun->isVarArg());
        extracted =
            bindFirstFuncArgument(builder, memFun, extracted, newFuncType);
        extracted->setName(extracted->getName().str() + "." + member);
      } else {
        return error("`{}` is not a field or member function "
                     "for struct `{}` at line {}",
                     member, structName, memberRef->state.row + 1);
      }
    }
    return extracted;
  }

  inline static std::optional<unsigned> getIndex(const llvm::Module& module,
                                                 llvm::StringRef structName,
                                                 llvm::StringRef memberName) {
    return getMetadataPartIndex(module, "structures", structName, memberName);
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_STRUCTMEMBER_HPP
