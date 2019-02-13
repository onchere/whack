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
#ifndef WHACK_ALIAS_HPP
#define WHACK_ALIAS_HPP

#pragma once

#include "../expressions/factors/ident.hpp"
#include "../types/typelist.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::codegen::elements {

class Alias final : public AST {
public:
  explicit Alias(const mpc_ast_t* const ast)
      : state_{ast->state}, identList_{getIdentList(ast->children[1])},
        typeList_{ast->children[3]} {}

  llvm::Error codegen(llvm::Module* const module) const {
    llvm::IRBuilder<> builder{module->getContext()};
    auto typeList = typeList_.codegen(builder);
    if (!typeList) {
      return typeList.takeError();
    }
    const auto& [types, variadic] = *typeList;
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
      if (auto err = add(module, identList_[i], types[i])) {
        return err;
      }
    }
    return llvm::Error::success();
  }

  inline void remove(llvm::Module* const module) const {
    for (size_t i = 0; i < identList_.size(); ++i) {
      remove(module, identList_[i]);
    }
  }

  static llvm::Error add(llvm::Module* const module, const llvm::StringRef name,
                         llvm::Type* const type) {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(module, name)) {
      return err;
    }
    (void)llvm::GlobalAlias::create(type, 0, llvm::GlobalAlias::ExternalLinkage,
                                    name, llvm::Constant::getNullValue(type),
                                    module);
    return llvm::Error::success();
  }

  inline static void remove(llvm::Module* const module,
                            const llvm::StringRef name) {
    if (auto alias = module->getNamedAlias(name)) {
      alias->setName(format(".tmp.{}", name.data()));
    }
  }

  static std::optional<llvm::Type*> get(const llvm::Module* const module,
                                        const llvm::StringRef typeName) {
    if (auto alias = module->getNamedAlias(typeName)) {
      return alias->getAliasee()->getType();
    }
    return std::nullopt;
  }

private:
  friend class AliasStmt;
  const mpc_state_t state_;
  const ident_list_t identList_;
  const types::TypeList typeList_;
};

class AliasStmt final : public stmts::Stmt {
public:
  explicit AliasStmt(const mpc_ast_t* const ast) : Stmt(kAlias), impl_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    for (const auto& ident : impl_.identList_) {
      using namespace expressions::factors;
      if (auto err = Ident::isUnique(builder, ident)) {
        return err;
      }
    }
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

} // end namespace whack::codegen::elements

#endif // WHACK_ALIAS_HPP
