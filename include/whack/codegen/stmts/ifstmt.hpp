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
#ifndef WHACK_IFSTMT_HPP
#define WHACK_IFSTMT_HPP

#pragma once

#include <folly/ScopeGuard.h>

namespace whack::codegen::stmts {

class If final : public Stmt {
public:
  explicit If(const mpc_ast_t* const ast) : Stmt(kIf) {
    const auto tag = getOutermostAstTag(ast->children[1]);
    if (tag == "letbind") {
      warning("pattern matching data classes not implemented");
    } else {
      condition_ =
          std::make_unique<expressions::operators::LogicalOr>(ast->children[1]);
      then_ = getStmt(ast->children[2]);
      if (ast->children_num > 3) {
        else_ = getStmt(ast->children[4]);
      }
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto cond = condition_->codegen(builder);
    if (!cond) {
      return cond.takeError();
    }
    auto condition = expressions::getLoadedValue(builder, *cond);
    if (!condition) {
      return condition.takeError();
    }
    const auto current = builder.GetInsertBlock();
    const auto func = current->getParent();
    auto& ctx = func->getContext();
    if (else_) {
      thenBlock_ = llvm::BasicBlock::Create(ctx, "then", func);
      elseBlock_ = llvm::BasicBlock::Create(ctx, "else", func);
      const auto contBlock = llvm::BasicBlock::Create(ctx, "cont", func);

      thenBlock_->moveAfter(current);
      builder.CreateCondBr(*condition, thenBlock_, elseBlock_);
      builder.SetInsertPoint(thenBlock_);
      if (auto err = then_->codegen(builder)) {
        return err;
      }
      thenBlock_ = builder.GetInsertBlock();
      if (!thenBlock_->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }

      elseBlock_->moveAfter(thenBlock_);
      builder.SetInsertPoint(elseBlock_);
      if (auto err = else_->codegen(builder)) {
        return err;
      }
      elseBlock_ = builder.GetInsertBlock();
      if (!elseBlock_->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }

      contBlock->moveAfter(elseBlock_);
      builder.SetInsertPoint(contBlock);

      // @todo PHI nodes?
    } else {
      thenBlock_ = llvm::BasicBlock::Create(ctx, "then", func);
      const auto contBlock = llvm::BasicBlock::Create(ctx, "cont", func);

      thenBlock_->moveAfter(current);
      builder.CreateCondBr(*condition, thenBlock_, contBlock);
      builder.SetInsertPoint(thenBlock_);
      if (auto err = then_->codegen(builder)) {
        return err;
      }
      thenBlock_ = builder.GetInsertBlock();
      if (!thenBlock_->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }

      contBlock->moveAfter(thenBlock_);
      builder.SetInsertPoint(contBlock);

      // @todo PHI nodes?
    }
    return llvm::Error::success();
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    builder.SetInsertPoint(thenBlock_);
    if (auto err = then_->runScopeExit(builder)) {
      return err;
    }
    if (else_) {
      builder.SetInsertPoint(elseBlock_);
      return else_->runScopeExit(builder);
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kIf;
  }

private:
  std::unique_ptr<expressions::operators::LogicalOr> condition_;
  std::unique_ptr<Stmt> then_, else_;
  mutable llvm::BasicBlock* thenBlock_;
  mutable llvm::BasicBlock* elseBlock_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_IFSTMT_HPP
