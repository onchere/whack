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
#ifndef WHACK_LETEXPR_HPP
#define WHACK_LETEXPR_HPP

#pragma once

#include "comparison.hpp"
#include "newexpr.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::ast {

class LetExpr final : public Stmt {
public:
  explicit LetExpr(const mpc_ast_t* const ast) : Stmt(kLetExpr), ast_{ast} {
    auto idx = 1;
    varsAreMut_ = std::string_view(ast->children[idx]->contents) == "mut";
    if (varsAreMut_) {
      ++idx;
    }
    identList_ = getIdentList(ast->children[idx]);
    exprListIndex_ = idx + 2; // to check if we have expression "new"
    exprList_ = getExprList(ast->children[idx + 2]);
    if (ast->children_num > idx + 2) {
      comparison_ = std::make_unique<Comparison>(ast->children[idx + 4]);
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (exprList_.size() > identList_.size()) {
      return error("invalid number of values to assign "
                   "to at line {}",
                   ast_->state.row + 1);
    }

    if (identList_.size() > exprList_.size()) {
      auto expr = exprList_[0]->codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      // @todo Constrain types allowed for expr??
      for (size_t i = 0; i < identList_.size(); ++i) {
        const auto& name = identList_[i];
        if (name == "_") { // we don't store the value
          continue;
        }
        if (auto err = Ident::isUnique(builder, name, ast_->state)) {
          return err;
        }
        const auto value = builder.CreateExtractValue(*expr, i, "");
        const auto alloc =
            builder.CreateAlloca(value->getType(), 0, nullptr, name);
        builder.CreateStore(value, alloc);
      }
    } else {
      for (size_t i = 0; i < identList_.size(); ++i) {
        auto val = exprList_[i]->codegen(builder);
        if (!val) {
          return val.takeError();
        }
        const auto value = *val;
        const auto& name = identList_[i];
        if (name == "_") { // we don't store the value
          continue;
        }
        if (auto err = Ident::isUnique(builder, name, ast_->state)) {
          return err;
        }

        const auto alloc =
            builder.CreateAlloca(value->getType(), 0, nullptr, name);
        builder.CreateStore(value, alloc);

        auto expr = ast_->children[exprListIndex_];
        if (exprList_.size() > 1) {
          const auto idx = i == 0 ? 0 : i + 2 * i - 1;
          expr = expr->children[idx];
        }

        if (getInnermostAstTag(expr) == "newexpr") { // @todo: Refactor
          // we add metadata to indicate this object is heap-allocated
          const auto func = builder.GetInsertBlock()->getParent();
          const auto module = func->getParent();
          llvm::MDBuilder MDBuilder{module->getContext()};
          const std::pair<llvm::MDNode*, uint64_t> metadata[] = {
              {reinterpret_cast<llvm::MDNode*>(MDBuilder.createString(name)),
               0}};
          const auto newedMD =
              MDBuilder.createTBAAStructTypeNode(func->getName(), metadata);
          module->getOrInsertNamedMetadata("new")->addOperand(newedMD);
        }
      }
    }
    return llvm::Error::success();
  }

  inline const auto& variables() const { return identList_; }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kLetExpr;
  }

private:
  const mpc_ast_t* const ast_;
  bool varsAreMut_{false};
  int exprListIndex_;
  ident_list_t identList_;
  small_vector<expr_t> exprList_;
  std::unique_ptr<Comparison> comparison_;
};

} // end namespace whack::ast

#endif // WHACK_LETEXPR_HPP
