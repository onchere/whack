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
#ifndef WHACK_RETURNSTMT_HPP
#define WHACK_RETURNSTMT_HPP

#pragma once

#include "ast.hpp"
#include "structure.hpp"
#include <folly/ScopeGuard.h>
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

class Return final : public Stmt {
public:
  explicit Return(const mpc_ast_t* const ast)
      : Stmt(kReturn), state_{ast->state} {
    if (ast->children_num > 2) {
      exprList_ = getExprList(ast->children[1]);
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (exprList_.empty()) {
      builder.CreateRetVoid();
    } else if (exprList_.size() == 1) {
      auto expr = exprList_.back()->codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      builder.CreateRet(getLoadedValue(builder, *expr));
    } else {
      small_vector<llvm::Value*> values;
      for (const auto& expression : exprList_) {
        auto expr = expression->codegen(builder);
        if (!expr) {
          return expr.takeError();
        }
        values.push_back(getLoadedValue(builder, *expr));
      }
      builder.CreateAggregateRet(values.data(),
                                 static_cast<unsigned int>(values.size()));
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kReturn;
  }

private:
  const mpc_state_t state_;
  small_vector<expr_t> exprList_;

  // @todo
  inline static llvm::Value* getLoadedValue(llvm::IRBuilder<>& builder,
                                            llvm::Value* const value) {
    if (llvm::isa<llvm::GetElementPtrInst>(value)) {
      return builder.CreateLoad(value);
    }
    return value;
  }
};

} // end namespace whack::ast

#endif // WHACK_RETURNSTMT_HPP
