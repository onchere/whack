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
#ifndef WHACK_FNTYPE_HPP
#define WHACK_FNTYPE_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class FnType final : public AST {
public:
  explicit constexpr FnType(const mpc_ast_t* const ast) noexcept : ast_{ast} {}

  llvm::FunctionType* codegen(const llvm::Module* const module) const {
    if (ast_->children_num < 4) {
      return llvm::FunctionType::get(BasicTypes["void"], false);
    }

    const auto getReturnType = [&]() -> llvm::Type* {
      if (ast_->children_num > 4) {
        const auto [returnTypes, variadic] =
            getTypeList(ast_->children[4], module);
        if (variadic) {
          warning("cannot use a variadic type as function "
                  "return type at line {}",
                  ast_->state.row + 1);
        }
        if (returnTypes.size() > 1) {
          return llvm::StructType::get(module->getContext(), returnTypes);
        }
        return returnTypes.back();
      }
      return BasicTypes["void"];
    };

    if (getOutermostAstTag(ast_->children[2]) == "typelist") {
      const auto [paramTypes, variadic] =
          getTypeList(ast_->children[2], module);
      return llvm::FunctionType::get(getReturnType(), paramTypes, variadic);
    }
    return llvm::FunctionType::get(getReturnType(), false);
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif
