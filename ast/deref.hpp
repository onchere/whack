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
#ifndef WHACK_DEREF_HPP
#define WHACK_DEREF_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Deref final : public Factor {
public:
  explicit Deref(const mpc_ast_t* const ast)
      : Factor(kDeref), state_{ast->state}, numLoads_{ast->children_num - 1},
        value_{getFactor(ast->children[numLoads_])} {}

  llvm::Expected<llvm::Value*> loadPointer(llvm::IRBuilder<>& builder) const {
    auto v = value_->codegen(builder);
    if (!v) {
      return v.takeError();
    }
    auto deref = *v;
    for (auto i = 0; i < numLoads_; ++i) {
      if (!deref->getType()->isPointerTy()) {
        return error("type error: cannot dereference a "
                     "non-pointer type at index {} of value at line {}",
                     i, state_.row + 1);
      }
      deref = builder.CreateLoad(deref);
    }
    return deref;
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto ptr = this->loadPointer(builder);
    if (!ptr) {
      return ptr.takeError();
    }
    return builder.CreateLoad(*ptr);
  }

private:
  const mpc_state_t state_;
  const int numLoads_;
  std::unique_ptr<Factor> value_;
};

} // end namespace whack::ast

#endif // WHACK_DEREF_HPP
