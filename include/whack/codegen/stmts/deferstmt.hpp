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
#ifndef WHACK_DEFERSTMT_HPP
#define WHACK_DEFERSTMT_HPP

#pragma once

#include <llvm/IR/CFG.h>

namespace whack::codegen::stmts {

class Defer final : public Stmt {
public:
  explicit Defer(const mpc_ast_t* const ast)
      : Stmt(kDefer), stmt_{getStmt(ast->children[1])} {}

  inline llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    deferBlock_ = builder.GetInsertBlock();
    return llvm::Error::success();
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    llvm::SmallPtrSet<llvm::BasicBlock* const, 10> terminators;
    std::function<void(llvm::BasicBlock* const)> apply;
    apply = [&](llvm::BasicBlock* const block) {
      if (llvm::succ_empty(block)) {
        terminators.insert(block);
        return;
      }
      for (const auto successor : llvm::successors(block)) {
        apply(successor);
      }
    };
    apply(deferBlock_);
    const auto run = [&](llvm::BasicBlock* const block) -> llvm::Error {
      auto& back = block->back();
      if (back.isTerminator()) {
        builder.SetInsertPoint(&back);
      } else {
        builder.SetInsertPoint(block);
      }
      if (auto err = stmt_->codegen(builder)) {
        return err;
      }
      return stmt_->runScopeExit(builder);
    };
    for (const auto block : terminators) {
      if (auto err = run(block)) {
        return err;
      }
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kDefer;
  }

private:
  std::unique_ptr<Stmt> stmt_;
  mutable llvm::BasicBlock* deferBlock_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_DEFERSTMT_HPP
