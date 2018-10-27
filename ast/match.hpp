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
#ifndef WHACK_MATCH_HPP
#define WHACK_MATCH_HPP

#pragma once

#include "ast.hpp"
#include "typelist.hpp"

namespace whack::ast {

class Match final : public Stmt {
public:
  explicit constexpr Match(const mpc_ast_t* const ast) noexcept
      : Stmt(kMatch), ast_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (getOutermostAstTag(ast_->children[5]) == "exprlist") {
      return this->exprMatch(builder);
    }
    // @todo Refactor
    return this->dataClassMatch(builder);
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kMatch;
  }

private:
  const mpc_ast_t* const ast_;

  llvm::Error exprMatch(llvm::IRBuilder<>& builder) const {
    using exprlist_t = small_vector<llvm::Value*>;
    using stmt_t = std::unique_ptr<Stmt>;

    auto val = getExpressionValue(ast_->children[2])->codegen(builder);
    if (!val) {
      return val.takeError();
    }
    const auto subject = *val;
    const auto type = subject->getType();
    small_vector<std::pair<exprlist_t, stmt_t>> options;
    stmt_t defaultStmt;
    small_vector<llvm::Value*> allOptions;

    for (auto i = 5; i < ast_->children_num - 1; i += 3) {
      const auto ref = ast_->children[i];
      if (std::string_view(ref->contents) == "default") {
        defaultStmt = getStmt(ast_->children[i + 2]);
      } else {
        exprlist_t values;
        for (const auto& expr : getExprList(ref)) {
          auto opt = expr->codegen(builder);
          if (!opt) {
            return opt.takeError();
          }
          const auto option = *opt;
          if (option->getType() != type) {
            return error("invalid type for match option at line {}",
                         ref->state.row + 1);
          }
          if (std::find(allOptions.begin(), allOptions.end(), option) !=
              allOptions.end()) {
            return error("duplicate option for match at line {}",
                         ref->state.row + 1);
          }
          values.push_back(option);
          allOptions.push_back(option);
        }
        options.emplace_back(
            std::pair{std::move(values), getStmt(ast_->children[i + 2])});
      }
    }

    const auto func = builder.GetInsertBlock()->getParent();
    auto& ctx = func->getContext();
    auto defaultBlock = llvm::BasicBlock::Create(ctx, "default", func);
    const auto contBlock = llvm::BasicBlock::Create(ctx, "block", func);
    const auto switcher =
        builder.CreateSwitch(subject, defaultBlock, options.size());

    for (const auto& [alternatives, stmt] : options) {
      auto caseBlock = llvm::BasicBlock::Create(ctx, "case", func);
      for (const auto& value : alternatives) {
        switcher->addCase(llvm::dyn_cast<llvm::ConstantInt>(value), caseBlock);
      }
      caseBlock->moveBefore(contBlock);
      builder.SetInsertPoint(caseBlock);
      if (auto err = stmt->codegen(builder)) {
        return err;
      }
      caseBlock = builder.GetInsertBlock();
      if (!caseBlock->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }
    }

    if (defaultStmt) {
      builder.SetInsertPoint(defaultBlock);
      if (auto err = defaultStmt->codegen(builder)) {
        return err;
      }
      defaultBlock = builder.GetInsertBlock();
      if (!defaultBlock->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }
    }

    builder.SetInsertPoint(contBlock);
    return llvm::Error::success();
  }

  llvm::Error patternMatch(llvm::IRBuilder<>& builder) const {
    // @todo: Proper pattern matching w/+ structured bindings
    return llvm::Error::success();
  }

  llvm::Error dataClassMatch(llvm::IRBuilder<>& builder) const {
    // @todo
    return llvm::Error::success();
  }
};

} // end namespace whack::ast

#endif // WHACK_MATCH_HPP
