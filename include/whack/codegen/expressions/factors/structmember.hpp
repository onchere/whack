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
#ifndef WHACK_STRUCTMEMBER_HPP
#define WHACK_STRUCTMEMBER_HPP

#pragma once

#include "../../types/type.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::codegen::expressions::factors {

class StructMember final : public Factor {
public:
  explicit constexpr StructMember(const mpc_ast_t* const ast) noexcept
      : Factor(kStructMember), ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto cont =
        getExpressionValue(ast_->children[0]->children[0])->codegen(builder);
    if (!cont) {
      return cont.takeError();
    }
    const auto memberRef = ast_->children[1];
    std::string member;
    if (getInnermostAstTag(memberRef) == "structopname") {
      auto name = getStructOpNameString(builder, getStructOpName(memberRef));
      if (!name) {
        return name.takeError();
      }
      member = std::move(*name);
    } else {
      member = memberRef->contents;
    }

    const auto container = *cont;
    auto type = container->getType();
    const auto typeError = [&] {
      return error("expected `{}` to be a struct type at line {}", member,
                   ast_->state.row + 1);
    };
    if (!type->isPointerTy()) {
      return typeError();
    } else {
      type = type->getPointerElementType();
      if (!type->isStructTy()) {
        return typeError();
      }
    }
    const auto structName = type->getStructName();
    const auto module = builder.GetInsertBlock()->getModule();
    llvm::Value* mem;
    if (const auto idx = getIndex(*module, structName, member)) {
      mem = builder.CreateStructGEP(type, container, idx.value(), member);
      if (structName.startswith("interface::")) { // @todo Necessary??
        mem = builder.CreateLoad(mem);
      }
    } else {
      const auto memFun = module->getFunction(
          format("struct::{}::{}", structName.data(), member));
      if (!memFun) {
        return error("`{}` is not a field or member function "
                     "for struct `{}` at line {}",
                     member, structName.str(), memberRef->state.row + 1);
      }
      mem = bindThis(builder, memFun, container);
    }
    const auto ret = builder.CreateAlloca(mem->getType(), 0, nullptr, "");
    builder.CreateStore(mem, ret);
    setIsDereferenceable(builder.getContext(), ret);
    return ret;
  }

  inline static llvm::Function* bindThis(llvm::IRBuilder<>& builder,
                                         llvm::Function* const memFun,
                                         llvm::Value* const thiz) {
    const auto newFuncType = llvm::FunctionType::get(
        memFun->getReturnType(),
        memFun->getFunctionType()->params().drop_front(), memFun->isVarArg());
    return bindFirstFuncArgument(builder, memFun, thiz, newFuncType);
  }

  inline static std::optional<unsigned>
  getIndex(const llvm::Module& module, const llvm::StringRef structName,
           const llvm::StringRef memberName) {
    return getMetadataPartIndex(module, "structures", structName, memberName);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kStructMember;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_STRUCTMEMBER_HPP
