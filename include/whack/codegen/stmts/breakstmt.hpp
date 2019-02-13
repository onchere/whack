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
#ifndef WHACK_BREAKSTMT_HPP
#define WHACK_BREAKSTMT_HPP

#pragma once

#include <llvm/IR/CFG.h>

namespace whack::codegen::stmts {

class Break final : public Stmt {
public:
  explicit constexpr Break(const mpc_ast_t* const ast) noexcept
      : Stmt(kBreak), state_{ast->state} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (!handleBreak(builder, builder.GetInsertBlock())) {
      return error("could not find a loop to break "
                   "out of at line {}",
                   state_.row + 1);
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kBreak;
  }

private:
  const mpc_state_t state_;

  static bool handleBreak(llvm::IRBuilder<>& builder,
                          llvm::BasicBlock* const current) {
    const auto currentName = current->getName();
    for (const auto pred : llvm::predecessors(current)) {
      llvm::BranchInst* branch = nullptr;
      if (llvm::isa<llvm::BranchInst>(pred->back())) {
        branch = &llvm::cast<llvm::BranchInst>(pred->back());
      }
      const auto name = pred->getName();
      if (name.startswith("while") || name.startswith("for")) {
        branch = llvm::cast<llvm::BranchInst>(
            pred->getSinglePredecessor()->getTerminator());
      }
      if (branch) {
        const auto succ0 = branch->getSuccessor(0);
        const auto succ1 = branch->getSuccessor(1);
        if (succ0->getName() == currentName) {
          builder.CreateBr(succ1);
          return true;
        } else if (succ1->getName() == currentName) {
          builder.CreateBr(succ0);
          return true;
        }
      }
      if (!llvm::pred_empty(pred)) {
        if (handleBreak(builder, pred)) {
          return true;
        }
      }
    }
    return false;
  }
};

} // end namespace whack::codegen::stmts

#endif // WHACK_BREAKSTMT_HPP
