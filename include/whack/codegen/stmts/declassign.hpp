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
#ifndef WHACK_DECLASSIGN_HPP
#define WHACK_DECLASSIGN_HPP

#pragma once

#include "../expressions/factors/initializer.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::codegen::stmts {

class DeclAssign final : public Stmt {
public:
  explicit DeclAssign(const mpc_ast_t* const ast) : Stmt(kDeclAssign) {
    if (getInnermostAstTag(ast) == "declassign") {
      type_ = types::Type{ast->children[0]->children[0]};
      llvm::StringRef var{ast->children[0]->children[1]->contents};
      vars_.push_back(var);
      for (auto i = 1; i < ast->children_num; ++i) {
        const auto curr = ast->children[i];
        if (getOutermostAstTag(curr) == "initializer") {
          using namespace expressions::factors;
          initializers_.emplace_back(
              std::pair{var, std::make_unique<Initializer>(curr)});
        } else if (getInnermostAstTag(curr) == "ident") {
          var = curr->contents;
          vars_.push_back(var);
        }
      }
    } else {
      type_ = types::Type{ast->children[0]};
      vars_.push_back(ast->children[1]->contents);
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto tp = type_.codegen(builder);
    if (!tp) {
      return tp.takeError();
    }
    const auto type = *tp;
    using namespace expressions::factors;
    for (const auto& var : vars_) {
      if (auto err = Ident::isUnique(builder, var)) {
        return err;
      }
      bool found = false;
      for (const auto& [varName, initializer] : initializers_) {
        if (var == varName) {
          auto init = initializer->codegen(builder, type);
          if (!init) {
            return init.takeError();
          }
          (*init)->setName(var);
          found = true;
          break;
        }
      }
      if (!found) {
        builder.CreateAlloca(type, 0, nullptr, var);
      }
    }
    return llvm::Error::success();
  }

  inline const auto type(llvm::IRBuilder<>& builder) const {
    return type_.codegen(builder);
  }

  inline const auto& variables() const { return vars_; }
  inline const auto& initializers() const { return initializers_; }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kDeclAssign;
  }

private:
  types::Type type_;
  small_vector<llvm::StringRef> vars_;
  using init_t = std::unique_ptr<expressions::factors::Initializer>;
  std::vector<std::pair<llvm::StringRef, init_t>> initializers_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_DECLASSIGN_HPP
