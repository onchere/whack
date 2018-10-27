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
#ifndef WHACK_ELEMENT_HPP
#define WHACK_ELEMENT_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Element final : public Factor {
public:
  explicit constexpr Element(const mpc_ast_t* const ast) noexcept : ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto e = getFactor(ast_->children[0])->codegen(builder);
    if (!e) {
      return e.takeError();
    }
    auto extracted = *e;
    for (auto i = 2; i < ast_->children_num; i += 3) {
      // @todo Constrain types accepted for index
      auto idx = getExpressionValue(ast_->children[i])->codegen(builder);
      if (!idx) {
        return idx.takeError();
      }
      const auto index = *idx;
      const auto type = extracted->getType()->getPointerElementType();
      if (type->isArrayTy() || type->isPointerTy()) {
        extracted =
            builder.CreateInBoundsGEP(extracted, {Integral::zero(), index});
      } else if (Type::isVariableLengthArray(type)) {
        auto elems = builder.CreateStructGEP(type, extracted, 1);
        extracted = builder.CreateInBoundsGEP(elems, {Integral::zero(), index});
      } else {
        extracted = builder.CreateGEP(extracted, index);
      }
    }
    return extracted;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_ELEMENT_HPP
