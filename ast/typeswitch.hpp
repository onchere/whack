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
#ifndef WHACK_TYPESWITCH_HPP
#define WHACK_TYPESWITCH_HPP

#pragma once

#include "ast.hpp"
#include "typelist.hpp"

namespace whack::ast {

class TypeSwitch final : public Stmt {
public:
  explicit constexpr TypeSwitch(const mpc_ast_t* const ast) noexcept
      : Stmt(kTypeSwitch), ast_{ast} {}

  // this is "constexpr"
  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    small_vector<std::pair<TypeList, std::unique_ptr<Stmt>>> options;
    std::unique_ptr<Stmt> defaultStmt;
    for (auto i = 6; i < ast_->children_num - 1; i += 3) {
      const auto ref = ast_->children[i];
      if (std::string_view(ref->contents) == "default") {
        defaultStmt = getStmt(ast_->children[i + 2]);
      } else {
        options.emplace_back(
            std::pair{TypeList{ref}, getStmt(ast_->children[i + 2])});
      }
    }
    auto e = getExpressionValue(ast_->children[3])->codegen(builder);
    if (!e) {
      return e.takeError();
    }
    const auto type = (*e)->getType();
    bool matched = false;
    const auto module = builder.GetInsertBlock()->getModule();
    for (const auto& [typeList, stmt] : options) {
      if (matched) {
        break;
      }
      const auto [types, variadic] = typeList.codegen(module);
      if (variadic) {
        return error("cannot use a variadic type in type switch "
                     "at line {}",
                     ast_->state.row + 1);
      }
      for (const auto t : types) {
        if (type == t) {
          matched = true;
          if (auto err = stmt->codegen(builder)) {
            return err;
          }
          if (auto err = stmt->runScopeExit(builder)) {
            return err;
          }
          break;
        }
      }
    }
    if (!matched && defaultStmt) {
      if (auto err = defaultStmt->codegen(builder)) {
        return err;
      }
      return defaultStmt->runScopeExit(builder);
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kTypeSwitch;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_TYPESWITCH_HPP
