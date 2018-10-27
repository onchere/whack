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
#ifndef WHACK_MEMBERINITLIST_HPP
#define WHACK_MEMBERINITLIST_HPP

#pragma once

#include "structmember.hpp"

namespace whack::ast {

class MemberInitList final : public AST {
public:
  explicit MemberInitList(const mpc_ast_t* const ast) {
    for (auto i = 1; i < ast->children_num - 1; i += 4) {
      std::string name{ast->children[i]->contents};
      for (const auto& [varName, _] : values_) {
        if (varName == name) {
          fatal("member `{}` already declared for the member "
                "initializer list at line {}",
                name, ast->state.row + 1);
        }
      }
      auto v = std::make_pair(std::move(name),
                              getExpressionValue(ast->children[i + 2]));
      values_.emplace_back(std::move(v));
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder,
                                       llvm::Type* const type,
                                       const mpc_state_t state) const {
    if (!type->isStructTy()) {
      return error("cannot use a member initializer list for type "
                   "at line {}",
                   state.row + 1);
    }
    const auto module = builder.GetInsertBlock()->getParent()->getParent();
    const auto structName = type->getStructName().str();
    const auto obj = builder.CreateAlloca(type, 0, nullptr, structName);
    for (const auto& [member, value] : values_) {
      if (const auto idx =
              StructMember::getIndex(*module, structName, member)) {
        const auto ptr =
            builder.CreateStructGEP(type, obj, idx.value(), member);
        auto v = value->codegen(builder);
        if (!v) {
          return v.takeError();
        }
        const auto val = *v;
        if (val->getType() != ptr->getType()->getPointerElementType()) {
          return error("type mismatch: cannot assign value to "
                       "field `{}` of struct `{}` at line {}",
                       member, structName, state.row + 1);
        }
        builder.CreateStore(val, ptr);
      } else {
        return error("field `{}` does not exist for struct `{}` at line {}",
                     member, structName, state.row + 1);
      }
    }
    return obj;
  }

private:
  using expr_t = std::unique_ptr<Expression>;
  std::vector<std::pair<std::string, expr_t>> values_;
};

} // end namespace whack::ast

#endif // WHACK_MEMBERINITLIST_HPP
