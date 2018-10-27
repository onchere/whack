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
#ifndef WHACK_INITLIST_HPP
#define WHACK_INITLIST_HPP

#pragma once

#include "ast.hpp"
#include "structmember.hpp"
#include "type.hpp"
#include <folly/Likely.h>

namespace whack::ast {

class InitList final : public AST {
public:
  explicit InitList(const mpc_ast_t* const ast) {
    if (ast->children_num > 2) {
      values_ = getExprList(ast->children[1]);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder,
                                       llvm::Type* const type,
                                       const mpc_state_t state) const {
    const auto [list, unique] = this->list(builder);
    if (list.empty()) {
      return llvm::Constant::getNullValue(type);
    }

    // Member-wise struct assignment
    if (!unique) {
      if (!type->isStructTy()) {
        return error("expected a struct type "
                     "at line {}",
                     state.row + 1);
      }
      auto ptr = builder.CreateAlloca(type, 0, nullptr, "");
      for (size_t i = 0; i < list.size(); ++i) {
        if (type->getStructElementType(i) != list[i]->getType()) {
          return error("element {} in initializer list does "
                       "not match correponding struct type "
                       "at line {}",
                       i, state.row + 1);
        }
        builder.CreateStore(list[i], builder.CreateStructGEP(type, ptr, i, ""));
      }
      return ptr;
    }

    // Variable-length array
    else if (Type::isVariableLengthArray(type)) {
      const auto eltType = type->getStructElementType(1)->getArrayElementType();
      // we check against first elem's type (list is homogenous)
      if (eltType != list[0]->getType()) {
        return error("type mismatch: cannot construct array at "
                     "line {}",
                     state.row + 1);
      }
      // @todo FIXME
      // %numElts = load i32* @SIZE
      // %val = alloca float, i32 %numElts
      auto size = llvm::ConstantInt::get(BasicTypes["int"], list.size());
      // auto elts = builder.CreateAlloca(eltType, size, nullptr, "");
      auto elts =
          llvm::ConstantArray::get(llvm::ArrayType::get(eltType, 0), list);
      const std::vector<llvm::Constant*> array = {
          size, llvm::dyn_cast<llvm::Constant>(elts)};
      const auto module = builder.GetInsertBlock()->getParent()->getParent();
      return llvm::ConstantStruct::getAnon(module->getContext(), array);
    }

    // Fixed-length array
    else if (type->isArrayTy()) { // @todo
      if (list[0]->getType() != type->getArrayElementType()) {
        return error("type mismatch: cannot initialize array "
                     "with the given values in initializer list "
                     "at line {}",
                     state.row + 1);
      }
      const auto len = type->getArrayNumElements();
      const auto numElem = list.size();
      if (numElem != len) {
        return error("type mismatch: expected {} array elements, "
                     "got {} at line {}",
                     len, numElem, state.row + 1);
      }
      return llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(type),
                                      list);
    }

    // @todo Support in AST
    else if (type->isVectorTy()) {
      if (list[0]->getType() != type->getVectorElementType()) {
        return error("type mismatch: cannot initialize vector "
                     "with the given values in initializer list "
                     "at line {}",
                     state.row + 1);
      }
      const auto len = type->getVectorNumElements();
      const auto numElem = list.size();
      if (numElem != len) {
        return error("type mismatch: expected {} vector elements, "
                     "got {} at line {}",
                     len, numElem, state.row + 1);
      }
      return llvm::ConstantVector::get({list});
    }

    // Struct/Scalar initialization
    else {
      if (type->isStructTy()) {
        auto ptr = builder.CreateAlloca(type, 0, nullptr, "");
        for (size_t i = 0; i < list.size(); ++i) {
          auto memberPtr = builder.CreateStructGEP(type, ptr, i, "");
          if (list[0]->getType() !=
              memberPtr->getType()->getPointerElementType()) {
            return error("type mismatch: cannot initialize type "
                         "with the given value in initializer list "
                         "at line {}",
                         state.row + 1);
          }
          builder.CreateStore(list[i], memberPtr);
        }
        return ptr;
      } else {
        if (list.size() > 1) {
          return error("cannot initialize the type with multiple "
                       "values at line {}",
                       state.row + 1);
        }
        if (list[0]->getType() != type) {
          return error("type mismatch: cannot initialize type "
                       "with the given value in initializer list "
                       "at line {}",
                       state.row + 1);
        }
        auto ptr = builder.CreateAlloca(type, 0, nullptr, "");
        builder.CreateStore(list[0], ptr);
        return ptr;
      }
    }
  }

private:
  small_vector<expr_t> values_;

  std::pair<small_vector<llvm::Constant*>, bool>
  list(llvm::IRBuilder<>& builder) const {
    small_vector<llvm::Constant*> values;
    llvm::Type* referenceType;
    bool unique = true;
    for (size_t i = 0; i < values_.size(); ++i) {
      // we always expect "concrete" values for this init list
      auto val = values_[i]->codegen(builder);
      if (!val) {
        llvm_unreachable("TODO: handle error"); // @todo
      }
      if (UNLIKELY(i == 0)) {
        referenceType = (*val)->getType();
      } else if ((*val)->getType() != referenceType) {
        unique = false;
      }
      values.push_back(llvm::dyn_cast<llvm::Constant>(*val));
    }
    return {std::move(values), unique};
  }
};

} // end namespace whack::ast

#endif // WHACK_INITLIST_HPP
