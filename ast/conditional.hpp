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
#ifndef WHACK_CONDITIONALS_HPP
#define WHACK_CONDITIONALS_HPP

#pragma once

#include "boolexpr.hpp"
#include "comparison.hpp"
#include "lexp.hpp"
#include "nullptr.hpp"

namespace whack::ast {

class Conditional final : public AST {
public:
  explicit constexpr Conditional(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const {
    const auto tag = getInnermostAstTag(ast_);
    if (tag == "comparison") {
      return Comparison{ast_}.codegen(builder);
    }
    if (tag == "boolexpr") {
      return BoolExpr{ast_}.codegen(builder);
    }
    auto val = Lexp{ast_}.codegen(builder);
    if (!val) {
      return val.takeError();
    }
    auto value = *val;
    const auto type = value->getType();
    if (type == BasicTypes["bool"]) {
      return value;
    }
    if (type->isPointerTy()) {
      const auto ptrType = type->getPointerElementType();
      if (ptrType->isStructTy()) {
        const auto funcName = // @todo Proper mangling...
            format("struct::{}::operator bool", ptrType->getStructName().str());
        const auto module = builder.GetInsertBlock()->getModule();
        if (const auto func = module->getFunction(funcName)) {
          return builder.CreateCall(func, value);
        }
      } else { // @todo
        return builder.CreateICmp(INTCMP["=="], value, NullPtr::get());
      }
    }
    return error("invalid type for conditional at line {}",
                 ast_->state.row + 1);
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_CONDITIONALS_HPP
