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
#ifndef WHACK_CONTINUESTMT_HPP
#define WHACK_CONTINUESTMT_HPP

#pragma once

#include <llvm/IR/CFG.h>

namespace whack::codegen::stmts {

class Continue final : public Stmt {
public:
  explicit constexpr Continue(const mpc_ast_t* const ast) noexcept
      : Stmt(kContinue), state_{ast->state} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (!handleContinue(builder, builder.GetInsertBlock())) {
      return error("could not find a loop to continue "
                   "with at line {}",
                   state_.row + 1);
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kContinue;
  }

private:
  const mpc_state_t state_;

  static bool handleContinue(llvm::IRBuilder<>& builder,
                             llvm::BasicBlock* const current) {
    const auto currentName = current->getName();
    if (currentName.startswith("while") || currentName.startswith("for")) {
      builder.CreateBr(current);
      return true;
    }
    for (const auto pred : llvm::predecessors(current)) {
      const auto name = pred->getName();
      if (name.startswith("while") || name.startswith("for")) {
        builder.CreateBr(pred);
        return true;
      } else {
        if (!llvm::pred_empty(pred)) {
          if (handleContinue(builder, pred)) {
            return true;
          }
        }
      }
    }
    return false;
  }
};

} // end namespace whack::codegen::stmts

#endif // WHACK_CONTINUESTMT_HPP
