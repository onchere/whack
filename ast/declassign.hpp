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
#ifndef WHACK_DECLASSIGN_HPP
#define WHACK_DECLASSIGN_HPP

#pragma once

#include "ast.hpp"
#include "ident.hpp"
#include "initializer.hpp"
#include "type.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

class DeclAssign final : public Stmt {
public:
  explicit DeclAssign(const mpc_ast_t* const ast)
      : Stmt(kDeclAssign), state_{ast->state},
        type_{ast->children[0]->children[0]} {
    std::string var{ast->children[0]->children[1]->contents};
    vars_.emplace_back(var);
    for (auto i = 1; i < ast->children_num - 1; ++i) {
      const auto& curr = ast->children[i];
      if (getOutermostAstTag(curr) == "initializer") {
        initializers_.emplace_back(std::make_pair(var, Initializer{curr}));
      } else if (getInnermostAstTag(curr) == "ident") {
        var = curr->contents;
        vars_.emplace_back(var);
      }
    }
  }

  /// @brief To be used in functions/basic block scope only!
  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    const auto type = type_.codegen(builder.GetInsertBlock()->getModule());
    for (const auto& var : vars_) {
      if (auto err = Ident::isUnique(builder, var, state_)) {
        return err;
      }
      const auto ptr = builder.CreateAlloca(type, 0, nullptr, var);
      for (const auto& [varName, initializer] : initializers_) {
        if (var == varName) {
          const auto list = initializer.list();
          llvm::Value* init;
          switch (list.index()) {
          case 0: // <initlist>
          {
            auto val = std::get<InitList>(list).codegen(builder, type, state_);
            if (!val) {
              return val.takeError();
            }
            init = *val;
            break;
          }
          case 1: // <listcomprehension>
          {
            auto val = std::get<ListComprehension>(list).codegen(builder, type,
                                                                 state_);
            if (!val) {
              return val.takeError();
            }
            init = *val;
            break;
          }
          default: // <memberinitlist>
          {
            auto val =
                std::get<MemberInitList>(list).codegen(builder, type, state_);
            if (!val) {
              return val.takeError();
            }
            init = *val;
            break;
          }
          }
          if (llvm::isa<llvm::AllocaInst>(init)) {
            init = builder.CreateLoad(init);
          }
          builder.CreateStore(init, ptr);
        }
      }
    }
    return llvm::Error::success();
  }

  inline const auto type(llvm::Module* const module) const {
    return type_.codegen(module);
  }

  inline const auto& variables() const { return vars_; }
  inline const auto& initializers() const { return initializers_; }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kDeclAssign;
  }

private:
  const mpc_state_t state_;
  Type type_;
  std::vector<std::string> vars_;
  std::vector<std::pair<std::string, Initializer>> initializers_;
};

} // end namespace whack::ast

#endif // WHACK_DECLASSIGN_HPP
