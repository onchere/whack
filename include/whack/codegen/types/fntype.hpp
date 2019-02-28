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
#ifndef WHACK_FNTYPE_HPP
#define WHACK_FNTYPE_HPP

#pragma once

namespace whack::codegen::types {

class FnType final : public AST {
public:
  explicit constexpr FnType(const mpc_ast_t* const ast) noexcept : ast_{ast} {}

  llvm::Expected<llvm::FunctionType*>
  codegen(llvm::IRBuilder<>& builder) const {
    if (ast_->children_num == 2) {
      return llvm::FunctionType::get(BasicTypes["void"], false);
    }

    const auto getReturnType = [&]() -> llvm::Expected<llvm::Type*> {
      if (ast_->children_num > 4) {
        auto typeList = getTypeList(ast_->children[4], builder);
        if (!typeList) {
          return typeList.takeError();
        }
        const auto& [returnTypes, variadic] = *typeList;
        if (variadic) {
          return error("cannot use a variadic type as function "
                       "return type at line {}",
                       ast_->state.row + 1);
        }
        if (returnTypes.size() > 1) {
          return llvm::StructType::get(builder.getContext(), returnTypes);
        }
        return returnTypes.back();
      }
      return BasicTypes["void"];
    };

    auto returnType = getReturnType();
    if (!returnType) {
      return returnType.takeError();
    }
    if (getOutermostAstTag(ast_->children[2]) == "typelist") {
      auto typeList = getTypeList(ast_->children[2], builder);
      if (!typeList) {
        return typeList.takeError();
      }
      const auto& [paramTypes, variadic] = *typeList;
      if (paramTypes[0] == BasicTypes["void"]) {
        if (paramTypes.size() != 1) {
          return error("invalid type list in context at line {}",
                       ast_->state.row + 1);
        }
      } else {
        return llvm::FunctionType::get(*returnType, paramTypes, variadic);
      }
    }
    return llvm::FunctionType::get(*returnType, false);
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::codegen::types

#endif // WHACK_FNTYPE_HPP
