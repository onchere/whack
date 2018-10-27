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
#ifndef WHACK_TYPE_HPP
#define WHACK_TYPE_HPP

#pragma once

#include "arraytype.hpp"
#include "fntype.hpp"
#include "integral.hpp"
#include <llvm/IR/ConstantRange.h>
#include <sstream>

namespace whack::ast {

// @todo
class Type final : public AST {
public:
  explicit constexpr Type(const mpc_ast_t* const ast) noexcept : ast_{ast} {}

  const auto str() const {
    std::function<std::string(const mpc_ast_t* const)> getStr;
    getStr = [&getStr](const mpc_ast_t* const ast) -> std::string {
      if (!ast->children_num) {
        return ast->contents;
      }
      std::ostringstream os;
      for (auto i = 0; i < ast->children_num; ++i) {
        os << getStr(ast->children[i]);
      }
      return os.str();
    };
    return getStr(ast_);
  }

  inline bool isMutable() const {
    return ast_->children_num &&
           std::string_view(ast_->children[0]->contents) == "mut";
  }

  llvm::Type* codegen(const llvm::Module* const module) const {
    auto ref = ast_;
    if (this->isMutable()) {
      ref = ast_->children[1];
    }

    const auto tag = getInnermostAstTag(ref);
    if (tag == "pointertype") {
      return getPointerType(ref, module);
    }

    if (tag == "fntype") {
      return FnType{ref}.codegen(module);
    }

    if (tag == "arraytype") {
      return ArrayType{ref}.codegen(module);
    }

    if (tag == "ident") {
      const auto& typeName = ref->contents;
      if (auto type = BasicTypes[typeName]) {
        return type;
      }

      if (auto type = module->getTypeByName(typeName)) {
        return type;
      }

      if (auto type =
              module->getTypeByName(format("interface::{}", typeName))) {
        return type; // @todo
      }

      if (auto MD = module->getNamedMetadata("aliases")) {
        for (unsigned i = 0; i < MD->getNumOperands(); ++i) {
          const auto operand = MD->getOperand(i);
          const auto name = reinterpret_cast<const llvm::MDString*>(
                                operand->getOperand(0).get())
                                ->getString();
          if (name == typeName) {
            return llvm::mdconst::dyn_extract<llvm::Constant>(
                       operand->getOperand(1))
                ->getType();
          }
        }
      }
    }

    if (tag == "identifier") {
      // @todo types from other module scopes
      llvm_unreachable("not implemented");
    }

    llvm_unreachable("invalid type tag kind!");
  }

  inline static auto getBitSize(const llvm::Module* const module,
                                llvm::Type* const type) {
    return module->getDataLayout().getTypeSizeInBits(type);
  }

  inline static bool isVariableLengthArray(const llvm::Type* const type) {
    return type->isStructTy() && type->getStructNumElements() == 2 &&
           type->getStructElementType(0) == BasicTypes["int"] &&
           type->getStructElementType(1)->isArrayTy() &&
           type->getStructElementType(1)->getArrayNumElements() == 0;
  }

  // @todo
  static std::pair<const llvm::/*Struct*/ Type*, bool>
  isStructKind(const llvm::Type* const type) {
    if (type->isStructTy()) {
      return {type, true};
    }
    if (type->isPointerTy()) {
      if (type->getPointerElementType()->isStructTy()) {
        return {type->getPointerElementType(), true};
      }
    }
    return {nullptr, false};
  }

  inline static bool isFunctionKind(const llvm::Type* const type) {
    return type->isFunctionTy() ||
           (type->isPointerTy() &&
            type->getPointerElementType()->isFunctionTy());
  }

  static llvm::Type* getPointerType(const mpc_ast_t* const ast,
                                    const llvm::Module* const module) {
    auto type = getType(ast->children[0], module);
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

  inline static llvm::Type* getReturnType(const llvm::Module* const module,
                                          const std::unique_ptr<Type>& type) {
    return type ? type->codegen(module) : BasicTypes["void"];
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
  const mpc_ast_t* const ast_;
};

static llvm::Type* getType(const mpc_ast_t* const ast,
                           const llvm::Module* const module) {
  const auto type = Type{ast}.codegen(module);
  if (Type::getUnderlyingType(type)->isFunctionTy()) {
    return type->getPointerTo(0);
  }
  return type;
}

} // end namespace whack::ast

#endif // WHACK_TYPE_HPP
