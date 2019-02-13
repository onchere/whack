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
#ifndef WHACK_DELETESTMT_HPP
#define WHACK_DELETESTMT_HPP

#pragma once

namespace whack::codegen::stmts {

class Delete final : public Stmt {
public:
  explicit Delete(const mpc_ast_t* const ast)
      : Stmt(kDelete), state_{ast->state}, exprList_{expressions::getExprList(
                                               ast->children[1])} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    const auto block = builder.GetInsertBlock();
    for (const auto& expr : exprList_) {
      auto e = expr->codegen(builder);
      if (!e) {
        return e.takeError();
      }
      auto source = *e;
      if (llvm::isa<llvm::AllocaInst>(source)) {
        source = builder.CreateLoad(source);
      }
      if (!source->getType()->isPointerTy()) {
        return error("invalid type for operator delete at line {}",
                     state_.row + 1);
      }
      if (!block->empty() && block->back().isTerminator()) {
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
  const small_vector<expr_t> exprList_;
};

} // namespace whack::codegen::stmts

#endif // WHACK_DELETESTMT_HPP
