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
#ifndef WHACK_ARRAYTYPE_HPP
#define WHACK_ARRAYTYPE_HPP

#pragma once

#include "ast.hpp"
#include "integral.hpp"

namespace whack::ast {

class ArrayType final : public AST {
public:
  explicit constexpr ArrayType(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  llvm::Type* codegen(const llvm::Module* const module) const {
    // Fixed-size array
    if (ast_->children_num == 4) {
      const Integral len{ast_->children[1]};
      const auto type = getType(ast_->children[3], module);
      return reinterpret_cast<llvm::Type*>(
          llvm::ArrayType::get(type, len.value()));
    }
    // Variable-length array
    return getVarLenType(module->getContext(),
                         getType(ast_->children[2], module));
  }

  inline static llvm::Type* getVarLenType(llvm::LLVMContext& ctx,
                                          llvm::Type* const type) {
    return reinterpret_cast<llvm::Type*>(llvm::StructType::get(
        ctx, {BasicTypes["int"], llvm::ArrayType::get(type, 0)}));
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif
