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
#ifndef WHACK_DEREF_HPP
#define WHACK_DEREF_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Deref final : public Factor {
public:
  explicit Deref(const mpc_ast_t* const ast)
      : Factor(kDeref), state_{ast->state}, value_{getFactor(ast)} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto ptr = value_->codegen(builder);
    if (!ptr) {
      return ptr.takeError();
    }
    auto ret = *ptr;
    const auto type = ret->getType();
    if (!type->isPointerTy()) {
      return error("type error: cannot dereference a "
                   "non-pointer type at line {}",
                   state_.row + 1);
    }
    if (llvm::isa<llvm::AllocaInst>(ret)) {
      ret = builder.CreateLoad(ret);
    }
    return builder.CreateLoad(ret);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kDeref;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<Factor> value_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_DEREF_HPP
