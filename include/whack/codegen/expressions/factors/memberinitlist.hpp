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
#ifndef WHACK_MEMBERINITLIST_HPP
#define WHACK_MEMBERINITLIST_HPP

#pragma once

#include "structmember.hpp"

namespace whack::codegen::expressions::factors {

class MemberInitList final : public Factor {
public:
  explicit MemberInitList(const mpc_ast_t* const ast)
      : Factor(kMemberInitList), state_{ast->state} {
    for (auto i = 0; i < ast->children_num; i += 4) {
      const llvm::StringRef name{ast->children[i]->contents};
      for (const auto& [varName, _] : values_) {
        if (varName == name) {
          warning("member `{}` already declared for the member "
                  "initializer list at line {}",
                  name.data(), state_.row + 1);
        }
      }
      values_.emplace_back(
          std::pair{name, getExpressionValue(ast->children[i + 2])});
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    small_vector<llvm::StringRef> typeNames;
    small_vector<llvm::Value*> values;
    small_vector<llvm::Type*> types;
    for (const auto& [member, value] : values_) {
      auto v = value->codegen(builder);
      if (!v) {
        return v.takeError();
      }
      typeNames.push_back(member);
      auto val = getLoadedValue(builder, *v);
      if (!val) {
        return val.takeError();
      }
      values.push_back(*val);
      types.push_back((*val)->getType());
    }
    const auto type = llvm::StructType::create(builder.getContext(), types,
                                               "::memberinitlist", true);
    const auto ret = builder.CreateAlloca(type, 0, nullptr, type->getName());
    for (size_t i = 0; i < values.size(); ++i) {
      const auto ptr = builder.CreateStructGEP(type, ret, i, "");
      builder.CreateStore(values[i], ptr);
    }
    addStructTypeMetadata(builder.GetInsertBlock()->getModule(), "structures",
                          type->getStructName(), typeNames);
    return ret;
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder,
                                       llvm::Type* const type) const {
    if (!type->isStructTy()) {
      return error("cannot use a member initializer list for type "
                   "at line {}",
                   state_.row + 1);
    }
    const auto module = builder.GetInsertBlock()->getParent()->getParent();
    const auto structName = type->getStructName().str();
    const auto obj = builder.CreateAlloca(type, 0, nullptr, structName);
    for (const auto& [member, value] : values_) {
      const auto idx = StructMember::getIndex(*module, structName, member);
      if (!idx) {
        return error("field `{}` does not exist for struct `{}` at line {}",
                     member.data(), structName, state_.row + 1);
      }
      const auto ptr = builder.CreateStructGEP(type, obj, idx.value(), member);
      auto v = value->codegen(builder);
      if (!v) {
        return v.takeError();
      }
      auto val = getLoadedValue(builder, *v, false);
      if (!val) {
        return val.takeError();
      }
      if ((*val)->getType() != ptr->getType()->getPointerElementType()) {
        return error("type mismatch: cannot assign value to "
                     "field `{}` of struct `{}` at line {}",
                     member.data(), structName, state_.row + 1);
      }
      builder.CreateStore(*val, ptr);
    }
    return obj;
  }

private:
  const mpc_state_t state_;
  std::vector<std::pair<llvm::StringRef, expr_t>> values_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_MEMBERINITLIST_HPP
