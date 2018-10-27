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
#ifndef WHACK_IFSTMT_HPP
#define WHACK_IFSTMT_HPP

#pragma once

#include "condition.hpp"
#include "letexpr.hpp"
#include <folly/ScopeGuard.h>

namespace whack::ast {

class If final : public Stmt {
public:
  explicit If(const mpc_ast_t* const ast) : Stmt(kIf) {
    const auto tag = getOutermostAstTag(ast->children[1]);
    if (tag == "letbind") {
      llvm_unreachable("pattern matching data classes not implemented");
    } else {
      auto idx = 1;
      while (getOutermostAstTag(ast->children[idx]) == "letexpr") {
        letExprs_.emplace_back(LetExpr{ast->children[idx]});
        ++idx;
      }
      condition_ = std::make_unique<Condition>(ast->children[idx++]);
      then_ = getStmt(ast->children[idx++]);
      if (ast->children_num > idx) {
        else_ = getStmt(ast->children[++idx]);
      }
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    for (const auto& let : letExprs_) {
      if (auto err = let.codegen(builder)) {
        return err;
      }
    }
    SCOPE_EXIT { // we "mangle" the let variables to restrict scope
      const auto symTbl = builder.GetInsertBlock()->getValueSymbolTable();
      for (const auto& let : letExprs_) {
        for (const auto& var : let.variables()) {
          const auto alloc = symTbl->lookup(var);
          alloc->setName(".tmp." + alloc->getName().str());
        }
      }
    };

    auto condition = condition_->codegen(builder);
    if (!condition) {
      return condition.takeError();
    }
    const auto func = builder.GetInsertBlock()->getParent();
    auto& ctx = func->getContext();
    if (else_) {
      thenBlock_ = llvm::BasicBlock::Create(ctx, "then", func);
      elseBlock_ = llvm::BasicBlock::Create(ctx, "else");
      const auto contBlock = llvm::BasicBlock::Create(ctx, "cont");

      builder.CreateCondBr(*condition, thenBlock_, elseBlock_);
      builder.SetInsertPoint(thenBlock_);
      if (auto err = then_->codegen(builder)) {
        return err;
      }
      thenBlock_ = builder.GetInsertBlock();
      if (!thenBlock_->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }

      func->getBasicBlockList().push_back(elseBlock_);
      builder.SetInsertPoint(elseBlock_);
      if (auto err = else_->codegen(builder)) {
        return err;
      }
      elseBlock_ = builder.GetInsertBlock();
      if (!elseBlock_->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }

      func->getBasicBlockList().push_back(contBlock);
      builder.SetInsertPoint(contBlock);

      // @todo PHI nodes?
    } else {
      thenBlock_ = llvm::BasicBlock::Create(ctx, "then", func);
      const auto contBlock = llvm::BasicBlock::Create(ctx, "cont");

      builder.CreateCondBr(*condition, thenBlock_, contBlock);
      builder.SetInsertPoint(thenBlock_);
      if (auto err = then_->codegen(builder)) {
        return err;
      }
      thenBlock_ = builder.GetInsertBlock();
      if (!thenBlock_->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }

      func->getBasicBlockList().push_back(contBlock);
      builder.SetInsertPoint(contBlock);

      // @todo PHI nodes?
    }
    return llvm::Error::success();
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    llvm::IRBuilder<>::InsertPointGuard{builder};
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
  std::vector<LetExpr> letExprs_;
  std::unique_ptr<Condition> condition_;
  std::unique_ptr<Stmt> then_, else_;
  mutable llvm::BasicBlock* thenBlock_;
  mutable llvm::BasicBlock* elseBlock_;
};

} // end namespace whack::ast

#endif // WHACK_IFSTMT_HPP
