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
#ifndef WHACK_ALIAS_HPP
#define WHACK_ALIAS_HPP

#pragma once

#include "ast.hpp"
#include "metadata.hpp"
#include "typelist.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::ast {

class Alias final : public AST {
public:
  explicit Alias(const mpc_ast_t* const ast)
      : state_{ast->state}, identList_{getIdentList(ast->children[1])},
        typeList_{ast->children[3]} {}

  llvm::Error codegen(llvm::Module* const module) const {
    const auto [types, variadic] = typeList_.codegen(module);
    if (variadic) {
      return error("variadic types not allowed in alias "
                   "list at line {}",
                   state_.row + 1);
    }
    if (types.size() != identList_.size()) {
      return error("invalid number of types for alias "
                   "list at line {} (expected {}, got {})",
                   state_.row + 1, identList_.size(), types.size());
    }
    for (size_t i = 0; i < types.size(); ++i) {
      if (auto err = add(module, identList_[i].data(), types[i], state_)) {
        return err;
      }
    }
    return llvm::Error::success();
  }

  inline void remove(llvm::Module* const module) const {
    for (size_t i = 0; i < identList_.size(); ++i) {
      remove(module, identList_[i].data());
    }
  }

  static llvm::Error add(llvm::Module* const module, llvm::StringRef name,
                         llvm::Type* const type, const mpc_state_t state = {}) {
    const auto aliases = getMetadataParts(*module, "aliases");
    if (std::find(aliases.begin(), aliases.end(), name) != aliases.end()) {
      return error("alias `{}` already exists in scope "
                   "at line {}",
                   name.str(), state.row + 1);
    }
    llvm::MDBuilder MDBuilder{module->getContext()};
    const auto domain = reinterpret_cast<llvm::MDNode*>(
        MDBuilder.createConstant(llvm::Constant::getNullValue(type)));
    const auto MD = module->getOrInsertNamedMetadata("aliases");
    MD->addOperand(MDBuilder.createAliasScope(name, domain));
    return llvm::Error::success();
  }

  inline static void remove(const llvm::Module* const module,
                            llvm::StringRef name) {
    if (auto alias = getMetadataOperand(*module, "aliases", name)) {
      // alias.value()->removeFromParent();
    }
  }

private:
  const mpc_state_t state_;
  ident_list_t identList_;
  TypeList typeList_;
};

class AliasStmt final : public Stmt {
public:
  explicit AliasStmt(const mpc_ast_t* const ast) : Stmt(kAlias), impl_{ast} {}

  inline llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    return impl_.codegen(builder.GetInsertBlock()->getModule());
  }

  inline llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    impl_.remove(builder.GetInsertBlock()->getModule());
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kAlias;
  }

private:
  const Alias impl_;
};

} // end namespace whack::ast

#endif // WHACK_ALIAS_HPP
