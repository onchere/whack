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
#ifndef WHACK_BODY_HPP
#define WHACK_BODY_HPP

#pragma once

#include "../tags.hpp"

namespace whack::codegen::stmts {

class Body final : public Stmt {
public:
  explicit Body(const mpc_ast_t* const ast) : Stmt(kBody), state_{ast->state} {
    auto idx = 1;
    if (getInnermostAstTag(ast->children[0]) == "tags") {
      tags_ = std::make_unique<Tags>(ast->children[0]);
      ++idx;
    }
    for (; idx < ast->children_num - 1; ++idx) {
      statements_.emplace_back(getStmt(ast->children[idx]));
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    for (const auto& stmt : statements_) {
      if (auto err = stmt->codegen(builder)) {
        return err;
      }
    }
    return llvm::Error::success();
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    for (const auto& stmt : statements_) {
      if (auto err = stmt->runScopeExit(builder)) {
        return err;
      }
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kBody;
  }

private:
  const mpc_state_t state_;
  std::unique_ptr<Tags> tags_;
  small_vector<std::unique_ptr<Stmt>> statements_;

  llvm::Error handleTags(llvm::IRBuilder<>& builder) const {
    static llvm::StringMap<llvm::Attribute::AttrKind> InternalTags{
        {"noinline", llvm::Attribute::AttrKind::NoInline},
        {"inline", llvm::Attribute::AttrKind::InlineHint},
        {"mustinline", llvm::Attribute::AttrKind::AlwaysInline},
        {"noreturn", llvm::Attribute::AttrKind::NoReturn}};

    const auto func = builder.GetInsertBlock()->getParent();
    for (const auto& [name, args] : tags_->get()) {
      if (name.index() == 0) { // <scoperes>
        llvm_unreachable("not implemented!");
      } else { // <ident>
        const auto& tag = std::get<expressions::factors::Ident>(name).name();
        if (!InternalTags.count(tag)) { // @todo: Other tag kinds
          return error("tag `{}` not implemented at line {}", tag,
                       state_.row + 1);
        }
        func->addFnAttr(InternalTags[tag]);
      }
    }
    return llvm::Error::success();
  }
};

} // end namespace whack::codegen::stmts

#endif // WHACK_BODY_HPP
