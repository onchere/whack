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
#ifndef WHACK_ARRAYTYPE_HPP
#define WHACK_ARRAYTYPE_HPP

#pragma once

#include "../fwd.hpp"

namespace whack::codegen::types {

class ArrayType final : public AST {
public:
  explicit constexpr ArrayType(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  llvm::Expected<llvm::Type*> codegen(llvm::IRBuilder<>& builder) const {
    using namespace expressions;
    auto len = getExpressionValue(ast_->children[1])->codegen(builder);
    if (!len) {
      return len.takeError();
    }
    auto type = getType(ast_->children[3], builder);
    if (!type) {
      return type.takeError();
    }
    return reinterpret_cast<llvm::Type*>(llvm::ArrayType::get(
        *type, llvm::dyn_cast<llvm::ConstantInt>(*len)->getZExtValue()));
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::codegen::types

#endif // WHACK_EXPRTYPE_HPP
