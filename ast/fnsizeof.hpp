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
#ifndef WHACK_FNSIZEOF_HPP
#define WHACK_FNSIZEOF_HPP

#pragma once

#include "ast.hpp"
#include <llvm/IR/DataLayout.h>

namespace whack::ast {

class FnSizeOf final : public Factor {
public:
  explicit FnSizeOf(const mpc_ast_t* const ast)
      : variadic_{getInnermostAstTag(ast->children[1]) == "expansion"},
        expr_{getExpressionValue(ast->children[variadic_ ? 3 : 2])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    llvm_unreachable("not implemented");
    const auto module = builder.GetInsertBlock()->getModule();
    if (variadic_) {
      // @todo
    } else {
      if (auto value = expr_->codegen(builder)) {
        // @todo: Return size of
        // @todo: Load in case of allocas e.t.c
        return Type::getBitSize(module, (*value)->getType());
      } else {
        // @todo: Check if we have a type name
      }
    }
  }

private:
  const bool variadic_;
  std::unique_ptr<Expression> expr_;
};

} // end namespace whack::ast

#endif // WHACK_FNSIZEOF_HPP
