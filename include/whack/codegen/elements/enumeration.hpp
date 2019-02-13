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
#ifndef WHACK_ENUMERATION_HPP
#define WHACK_ENUMERATION_HPP

#pragma once

#include "../expressions/factors/integral.hpp"
#include "alias.hpp"

namespace whack::codegen::elements {

class Enumeration final : public AST {
public:
  explicit Enumeration(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[0]->contents} {
    const auto ref = ast->children[1];
    auto idx = 2;
    if (std::string_view(ref->children[1]->contents) == ":") {
      underlyingType_ = std::make_unique<types::Type>(ref->children[2]);
      idx += 2;
    }
    options_ = getIdentList(ref->children[idx]);
  }

  llvm::Error codegen(llvm::Module* const module) const {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(module, name_)) {
      return err;
    }
    llvm::Type* type;
    if (underlyingType_) {
      llvm::IRBuilder<> builder{module->getContext()};
      auto underlying = underlyingType_->codegen(builder);
      if (!underlying) {
        return underlying.takeError();
      }
      type = *underlying;
    } else {
      type = BasicTypes["int"];
    }
    if (!type->isIntegerTy()) {
      return error("cannot use a non-integral type for "
                   "enum values at line {}",
                   state_.row + 1);
    }
    if (options_.size() > (2 ^ types::Type::getBitSize(module, type))) {
      return error("type provided for enum `{}` has insufficient "
                   "size to hold the enum values at line {}",
                   name_, state_.row + 1);
    }
    for (size_t i = 0; i < options_.size(); ++i) {
      const auto option = format("{}::{}", name_, options_[i].data());
      if (module->getNamedGlobal(option)) {
        return error("enum option `{}` already exists at line {}", option,
                     state_.row + 1);
      }
      const auto value = Integral::get<llvm::Constant>(i, type);
      constexpr static auto linkage = llvm::GlobalVariable::ExternalLinkage;
      module->getGlobalList().push_back(
          new llvm::GlobalVariable{type, true, linkage, value, option});
    }
    // using EnumName = UnderlyingType;
    if (auto err = Alias::add(module, name_, type)) {
      return err;
    }
    return llvm::Error::success();
  }

  inline const auto& name() const { return name_; }

private:
  friend class EnumerationStmt;
  const mpc_state_t state_;
  const std::string name_;
  std::unique_ptr<types::Type> underlyingType_;
  ident_list_t options_;
};

class EnumerationStmt final : public stmts::Stmt {
public:
  explicit EnumerationStmt(const mpc_ast_t* const ast)
      : Stmt(kEnumeration), impl_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(builder, impl_.name_)) {
      return err;
    }
    return impl_.codegen(builder.GetInsertBlock()->getModule());
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    for (const auto& option : impl_.options_) {
      const auto opt = format("{}::{}", impl_.name_, option.data());
      if (const auto GV = module->getNamedGlobal(opt)) {
        GV->setName(".tmp." + GV->getName().str());
      }
    }
    Alias::remove(module, impl_.name_);
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kEnumeration;
  }

private:
  const Enumeration impl_;
};

} // end namespace whack::codegen::elements

#endif // WHACK_ENUMERATION_HPP
