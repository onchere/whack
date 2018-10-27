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
#ifndef WHACK_DEFERSTMT_HPP
#define WHACK_DEFERSTMT_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Defer final : public Stmt {
public:
  explicit Defer(const mpc_ast_t* const ast)
      : Stmt(kDefer), stmt_{getStmt(ast->children[1])} {}

  inline llvm::Error codegen(llvm::IRBuilder<>&) const final {
    return llvm::Error::success();
  }

  inline llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    builder.SetInsertPoint(&builder.GetInsertBlock()->back()); // @todo
    return stmt_->codegen(builder);
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kDefer;
  }

private:
  std::unique_ptr<Stmt> stmt_;
};

} // end namespace whack::ast

#endif // WHACK_DEFERSTMT_HPP
