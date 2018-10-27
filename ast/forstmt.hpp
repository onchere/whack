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
#ifndef WHACK_FORSTMT_HPP
#define WHACK_FORSTMT_HPP

#pragma once

#include "ast.hpp"
#include "forinexpr.hpp"
#include "letexpr.hpp"
#include <folly/ScopeGuard.h>
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

class For final : public Stmt {
  class ForIncrExpr {
  public:
    explicit ForIncrExpr(const mpc_ast_t* const ast)
        : let{ast->children[1]}, comparison{ast->children[2]} {
      for (auto i = 4; i < ast->children_num; i += 2) {
        const auto incr = ast->children[i];
        if (getInnermostAstTag(incr) == "preop") {
          steps.emplace_back(std::make_unique<PreOpStmt>(incr));
        } else { // <postop>
          steps.emplace_back(std::make_unique<PostOpStmt>(incr));
        }
      }
    }

  private:
    friend class For;
    const LetExpr let;
    const Comparison comparison;
    small_vector<std::unique_ptr<Stmt>> steps;
  };

public:
  explicit For(const mpc_ast_t* const ast)
      : Stmt(kFor), expr_{ast->children[0]}, stmt_{getStmt(ast->children[1])} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    const auto func = builder.GetInsertBlock()->getParent();
    auto& ctx = func->getContext();
    if (getInnermostAstTag(expr_) == "forinexpr") {
      llvm_unreachable("forinexpr not implemented");
    } else { // <forincrexpr>
      const auto body = llvm::BasicBlock::Create(ctx, "for", func);
      const auto cont = llvm::BasicBlock::Create(ctx, "cont", func);
      const ForIncrExpr expr{expr_};
      if (auto err = expr.let.codegen(builder)) {
        return err;
      }
      SCOPE_EXIT {
        const auto symTbl = builder.GetInsertBlock()->getValueSymbolTable();
        for (const auto& var : expr.let.variables()) {
          const auto alloc = symTbl->lookup(var);
          // we restrict scope of let variables via renaming
          alloc->setName(".tmp." + alloc->getName().str());
        }
      };
      auto comparison = expr.comparison.codegen(builder);
      if (!comparison) {
        return comparison.takeError();
      }
      builder.CreateCondBr(*comparison, body, cont);
      builder.SetInsertPoint(body);
      if (auto err = stmt_->codegen(builder)) {
        return err;
      }
      for (const auto& step : expr.steps) {
        if (auto err = step->codegen(builder)) {
          return err;
        }
      }
      comparison = expr.comparison.codegen(builder);
      if (!comparison) {
        return comparison.takeError();
      }
      builder.CreateCondBr(*comparison, body, cont);
      builder.SetInsertPoint(cont);
    }
    return llvm::Error::success();
  }

  inline llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    return stmt_->runScopeExit(builder);
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kFor;
  }

private:
  const mpc_ast_t* const expr_;
  std::unique_ptr<Stmt> stmt_;
};

} // end namespace whack::ast

#endif // WHACK_FORSTMT_HPP
