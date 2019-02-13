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
#ifndef WHACK_WHILESTMT_HPP
#define WHACK_WHILESTMT_HPP

#pragma once

namespace whack::codegen::stmts {

class While final : public Stmt {
public:
  explicit While(const mpc_ast_t* const ast)
      : Stmt(kWhile), condition_{ast->children[1]}, stmt_{getStmt(
                                                        ast->children[2])} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    const auto func = builder.GetInsertBlock()->getParent();
    auto& ctx = builder.getContext();
    const auto block = llvm::BasicBlock::Create(ctx, "while", func);
    cont_ = llvm::BasicBlock::Create(ctx, "cont", func);
    auto cond = condition_.codegen(builder);
    if (!cond) {
      return cond.takeError();
    }
    builder.CreateCondBr(*cond, block, cont_);
    builder.SetInsertPoint(block);
    if (auto err = stmt_->codegen(builder)) {
      return err;
    }
    if (!builder.GetInsertBlock()->back().isTerminator()) {
      cond = condition_.codegen(builder);
      if (!cond) {
        return cond.takeError();
      }
      builder.CreateCondBr(*cond, block, cont_);
    }
    cont_->moveAfter(builder.GetInsertBlock());
    builder.SetInsertPoint(cont_);
    return llvm::Error::success();
  }

  inline llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    llvm::IRBuilder<>::InsertPointGuard{builder};
    builder.SetInsertPoint(cont_);
    return stmt_->runScopeExit(builder);
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kWhile;
  }

private:
  const expressions::operators::LogicalOr condition_;
  const std::unique_ptr<Stmt> stmt_;
  mutable llvm::BasicBlock* cont_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_WHILESTMT_HPP
