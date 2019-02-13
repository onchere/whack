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
#ifndef WHACK_TYPESWITCH_HPP
#define WHACK_TYPESWITCH_HPP

#pragma once

#include "../types/exprtype.hpp"

namespace whack::codegen::stmts {

class TypeSwitch final : public Stmt {
public:
  explicit TypeSwitch(const mpc_ast_t* const ast)
      : Stmt(kTypeSwitch), state_{ast->state}, type_{ast->children[1]} {
    for (auto i = 3; i < ast->children_num - 1; i += 3) {
      const auto ref = ast->children[i];
      if (std::string_view(ref->contents) == "default") {
        defaultStmt_ = getStmt(ast->children[i + 2]);
      } else {
        options_.emplace_back(
            std::pair{types::TypeList{ref}, getStmt(ast->children[i + 2])});
      }
    }
  }

  // this is "constexpr"
  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto type = type_.codegen(builder);
    if (!type) {
      return type.takeError();
    }
    bool matched = false;
    for (const auto& [typeList, stmt] : options_) {
      if (matched) {
        break;
      }
      auto types = typeList.codegen(builder);
      if (!types) {
        return types.takeError();
      }
      const auto& [list, variadic] = *types;
      if (variadic) {
        return error("cannot use a variadic type in type switch "
                     "at line {}",
                     state_.row + 1);
      }
      for (const auto t : list) {
        if (*type == t) {
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
    if (!matched && defaultStmt_) {
      if (auto err = defaultStmt_->codegen(builder)) {
        return err;
      }
      return defaultStmt_->runScopeExit(builder);
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kTypeSwitch;
  }

private:
  const mpc_state_t state_;
  const types::ExprType type_;
  small_vector<std::pair<types::TypeList, std::unique_ptr<Stmt>>> options_;
  std::unique_ptr<Stmt> defaultStmt_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_TYPESWITCH_HPP
