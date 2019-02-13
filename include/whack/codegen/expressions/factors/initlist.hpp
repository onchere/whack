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
#ifndef WHACK_INITLIST_HPP
#define WHACK_INITLIST_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class InitList final : public Factor {
public:
  explicit InitList(const mpc_ast_t* const ast)
      : Factor(kInitList), state_{ast->state} {
    if (ast->children_num > 2) {
      values_ = getExprList(ast->children[1]);
    }
  }

  inline llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>& builder) const final {
    return this->codegen(builder, std::nullopt);
  }

  llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>& builder,
          std::optional<llvm::Type* const> expectedType) const {
    auto valueList = this->getList(builder);
    if (!valueList) {
      return valueList.takeError();
    }
    const auto& [list, homogenous] = *valueList;
    if (list.empty()) {
      if (!expectedType) {
        return error("type required in context: cannot zero-initialize "
                     "without a type at line {}",
                     state_.row + 1);
      }
      return llvm::Constant::getNullValue(*expectedType);
    }

    llvm::Type* type;
    if (expectedType) {
      type = *expectedType;
    } else {
      if (list.size() == 1) {
        type = list[0]->getType();
      } else if (homogenous) {
        type = llvm::ArrayType::get(list[0]->getType(), list.size());
      } else {
        small_vector<llvm::Type*> types;
        for (size_t i = 0; i < list.size(); ++i) {
          types.push_back(list[i]->getType());
        }
        type = llvm::StructType::get(builder.getContext(), types);
      }
    }

    // Member-wise struct init
    if (!homogenous) {
      if (!type->isStructTy()) {
        return error("expected a struct type at line {}", state_.row + 1);
      }
      auto ptr = builder.CreateAlloca(type, 0, nullptr, "");
      for (size_t i = 0; i < list.size(); ++i) {
        if (type->getStructElementType(i) != list[i]->getType()) {
          return error("element {} in initializer list does "
                       "not match corresponding struct element type "
                       "at line {}",
                       i, state_.row + 1);
        }
        builder.CreateStore(list[i], builder.CreateStructGEP(type, ptr, i, ""));
      }
      return builder.CreateLoad(ptr);
    }

    // Fixed-length array
    if (type->isArrayTy()) {
      if (list[0]->getType() != type->getArrayElementType()) {
        return error("type mismatch: cannot initialize array "
                     "with the given values in initializer list "
                     "at line {}",
                     state_.row + 1);
      }
      const auto len = type->getArrayNumElements();
      const auto numElem = list.size();
      if (numElem != len) {
        return error("type mismatch: expected {} array elements, "
                     "got {} at line {}",
                     len, numElem, state_.row + 1);
      }
      const auto ptr = builder.CreateAlloca(type, 0, nullptr, "");
      for (size_t i = 0; i < list.size(); ++i) {
        const auto idxPtr = builder.CreateInBoundsGEP(
            ptr, {Integral::get(0), Integral::get(i)});
        builder.CreateStore(list[i], idxPtr);
      }
      return builder.CreateLoad(ptr);
    }

    // Scalar init
    if (list.size() > 1) {
      return error("cannot initialize the type with initializer "
                   "values at line {}",
                   state_.row + 1);
    }
    if (list[0]->getType() != type) {
      return error("type mismatch: cannot initialize type "
                   "with the given value in initializer list "
                   "at line {}",
                   state_.row + 1);
    }
    return list[0];
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kInitList;
  }

private:
  const mpc_state_t state_;
  small_vector<expr_t> values_;

  using list_t = std::pair<small_vector<llvm::Value*>, bool>;
  llvm::Expected<list_t> getList(llvm::IRBuilder<>& builder) const {
    small_vector<llvm::Value*> values;
    llvm::Type* referenceType;
    bool homogenous = true;
    for (size_t i = 0; i < values_.size(); ++i) {
      // we always expect "concrete" values for this init list
      auto val = values_[i]->codegen(builder);
      if (!val) {
        return val.takeError();
      }
      const auto value = *val;
      if (i == 0) {
        referenceType = value->getType();
      } else if (value->getType() != referenceType) {
        homogenous = false;
      }
      values.push_back(value);
    }
    return list_t{std::move(values), homogenous};
  }
};

} // namespace whack::codegen::expressions::factors

#endif // WHACK_INITLIST_HPP
