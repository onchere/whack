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
#ifndef WHACK_BODY_HPP
#define WHACK_BODY_HPP

#pragma once

#include "ast.hpp"
#include "deferstmt.hpp"

namespace whack::ast {

class Body final : public Stmt {
public:
  explicit Body(const mpc_ast_t* const ast) : Stmt(kBody) {
    for (auto i = 1; i < ast->children_num - 1; ++i) {
      statements_.emplace_back(getStmt(ast->children[i]));
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    for (const auto& stmt : statements_) {
      if (auto err = stmt->codegen(builder)) {
        return err;
      }
      if (llvm::isa<Defer>(stmt.get())) {
        deferrals_.insert(deferrals_.begin(),
                          std::pair{builder.GetInsertBlock(), stmt.get()});
      }
    }
    return llvm::Error::success();
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    // llvm::IRBuilder<>::InsertPointGuard{builder};
    const auto current = builder.GetInsertBlock();
    for (const auto& stmt : statements_) {
      if (!llvm::isa<Defer>(stmt.get())) {
        if (auto err = stmt->runScopeExit(builder)) {
          return err;
        }
      }
    }

    for (const auto& [block, defer] : deferrals_) {
      if (block == current) {
        builder.SetInsertPoint(block);
        if (auto err = defer->runScopeExit(builder)) {
          return err;
        }
      } else {
        for (auto curr = block->getIterator(); curr != ++current->getIterator();
             ++curr) {
          auto b = &*curr;
          if (llvm::isa<llvm::ReturnInst>(b->back())) {
            builder.SetInsertPoint(b);
            if (auto err = defer->runScopeExit(builder)) {
              return err;
            }
          } else {
            llvm_unreachable("invalid defer kind?!"); // @todo
          }
        }
      }
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kBody;
  }

private:
  small_vector<std::unique_ptr<Stmt>> statements_;
  using deferral_info_t = std::pair<llvm::BasicBlock*, Stmt*>;
  mutable std::vector<deferral_info_t> deferrals_;
};

} // end namespace whack::ast

#endif // WHACK_BODY_HPP
