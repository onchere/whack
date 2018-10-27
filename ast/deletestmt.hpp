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
#ifndef WHACK_DELETESTMT_HPP
#define WHACK_DELETESTMT_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Delete final : public Stmt {
public:
  explicit Delete(const mpc_ast_t* const ast)
      : Stmt(kDelete), state_{ast->state}, exprList_{
                                               getExprList(ast->children[1])} {}

  // @todo
  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    const auto block = builder.GetInsertBlock();
    for (const auto& expr : exprList_) {
      auto e = expr->codegen(builder);
      if (!e) {
        return e.takeError();
      }
      const auto source = *e;
      if (!source->getType()->isPointerTy()) {
        return error("invalid type for operator delete at line {}",
                     state_.row + 1);
      }
      // @todo Refactor
      if (llvm::isa<llvm::ReturnInst>(block->back())) {
        (void)llvm::CallInst::CreateFree(source, &block->back());
      } else {
        builder.Insert(llvm::CallInst::CreateFree(source, block));
      }
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kDelete;
  }

private:
  const mpc_state_t state_;
  small_vector<expr_t> exprList_;
};

} // end namespace whack::ast

#endif // WHACK_DELETESTMT_HPP
