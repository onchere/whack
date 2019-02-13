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
#ifndef WHACK_TYPE_HPP
#define WHACK_TYPE_HPP

#pragma once

#include "arraytype.hpp"
#include "exprtype.hpp"
#include "fntype.hpp"
#include <llvm/Support/raw_ostream.h>

namespace whack::codegen::types {

// @todo
class Type final : public AST {
public:
  explicit constexpr Type(const mpc_ast_t* const ast) noexcept : ast_{ast} {}

  constexpr Type() noexcept = default;

  llvm::Expected<llvm::Type*> codegen(llvm::IRBuilder<>& builder) const {
    assert(ast_);
    const auto tag = getInnermostAstTag(ast_);
    if (tag == "pointertype") {
      return getPointerType(ast_, builder);
    }

    if (tag == "fntype") {
      return FnType{ast_}.codegen(builder);
    }

    if (tag == "exprtype") {
      return ExprType{ast_}.codegen(builder);
    }

    if (tag == "arraytype") {
      return ArrayType{ast_}.codegen(builder);
    }

    if (tag == "overloadid" || tag == "scoperes" || tag == "ident") {
      const auto identifier = expressions::factors::getIdentifierString(ast_);
      llvm::Module* module;
      if (const auto block = builder.GetInsertBlock()) {
        module = block->getModule();
      } else {
        module = *builder.getContext().pImpl->OwnedModules.begin();
      }
      if (auto type = getFromTypeName(module, identifier)) {
        return type.value();
      }
      // we retry for struct funcs...
      if (auto func = module->getFunction(
              format("struct::{}", std::move(identifier)))) {
        return func->getType();
      }
    }
    return error("type does not exist in scope at line {}",
                 ast_->state.row + 1);
  }

  const bool isUnsignedIntTy() const {
    assert(ast_);
    if (getInnermostAstTag(ast_) != "ident") {
      return false;
    }
    const std::string_view view{ast_->contents};
    constexpr static auto variants = {"uint8",  "uint16", "uint",
                                      "uint32", "uint64", "uint128"};
    return std::find(variants.begin(), variants.end(), view) != variants.end();
  }

  static std::optional<llvm::Type*>
  getFromTypeName(const llvm::Module* const module,
                  const llvm::StringRef typeName) {
    if (auto type = BasicTypes[typeName]) {
      return type;
    }

    if (auto type = module->getTypeByName(typeName)) {
      return type;
    }

    if (auto type =
            module->getTypeByName(format("interface::{}", typeName.data()))) {
      return type;
    }

    if (auto type =
            module->getTypeByName(format("class::{}", typeName.data()))) {
      return type;
    }

    if (auto alias = module->getNamedAlias(typeName)) {
      return alias->getAliasee()->getType();
    }
    return std::nullopt;
  }

  inline static auto getBitSize(const llvm::Module* const module,
                                llvm::Type* const type) {
    return module->getDataLayout().getTypeSizeInBits(type);
  }

  inline static auto getByteSize(const llvm::Module* const module,
                                 llvm::Type* const type) {
    return getBitSize(module, type) / 8;
  }

  static std::pair<llvm::Type*, bool> isStructKind(llvm::Type* const type) {
    if (type->isStructTy()) {
      return {type, true};
    }
    if (type->isPointerTy() && type->getPointerElementType()->isStructTy()) {
      return {type->getPointerElementType(), true};
    }
    return {type, false};
  }

  inline static bool isFunctionKind(const llvm::Type* const type) {
    return type->isFunctionTy() ||
           (type->isPointerTy() &&
            type->getPointerElementType()->isFunctionTy());
  }

  static llvm::Expected<llvm::Type*>
  getPointerType(const mpc_ast_t* const ast, llvm::IRBuilder<>& builder) {
    auto tp = getType(ast->children[0], builder);
    if (!tp) {
      return tp.takeError();
    }
    auto type = *tp;
    for (auto i = 1; i < ast->children_num; ++i) {
      type = type->getPointerTo(0);
    }
    return type;
  }

  static llvm::Type* getUnderlyingType(llvm::Type* const type) {
    auto ret = type;
    while (ret->isPointerTy()) {
      ret = ret->getPointerElementType();
    }
    return ret;
  }

  inline static llvm::Expected<llvm::Type*>
  getReturnType(llvm::IRBuilder<>& builder, const std::unique_ptr<Type>& type) {
    return type ? type->codegen(builder) : BasicTypes["void"];
  }

  static llvm::Expected<llvm::Type*>
  getReturnType(llvm::LLVMContext& ctx,
                const std::optional<typelist_t>& returnTypeList,
                const mpc_state_t state = {}) {
    if (!returnTypeList) {
      return BasicTypes["void"];
    }
    const auto& [types, variadic] = *returnTypeList;
    if (variadic) {
      return error("cannot use a variadic return type "
                   "for function at line {}",
                   state.row + 1);
    }
    if (types.size() > 1) {
      return llvm::StructType::get(ctx, types);
    }
    return types.back();
  }

private:
  const mpc_ast_t* ast_{nullptr};
};

static llvm::Expected<llvm::Type*> getType(const mpc_ast_t* const ast,
                                           llvm::IRBuilder<>& builder) {
  auto tp = Type{ast}.codegen(builder);
  if (!tp) {
    return tp.takeError();
  }
  const auto type = *tp;
  if (Type::getUnderlyingType(type)->isFunctionTy()) {
    return type->getPointerTo(0);
  }
  return type;
}

} // end namespace whack::codegen::types

#endif // WHACK_TYPE_HPP
