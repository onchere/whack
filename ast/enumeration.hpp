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
#ifndef WHACK_ENUMERATION_HPP
#define WHACK_ENUMERATION_HPP

#pragma once

#include "alias.hpp"
#include "ast.hpp"
#include "integral.hpp"
#include "type.hpp"
#include <llvm/IR/ConstantRange.h>

namespace whack::ast {

class Enumeration final : public AST {
public:
  explicit Enumeration(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[1]->contents} {
    const auto ref = ast->children[2];
    auto idx = 2;
    if (std::string_view(ref->children[1]->contents) == ":") {
      underlyingType_ = std::make_unique<Type>(ref->children[2]);
      idx += 2;
    }
    options_ = getIdentList(ref->children[idx]);
  }

  llvm::Error codegen(llvm::Module* const module) const {
    const auto type =
        underlyingType_ ? underlyingType_->codegen(module) : BasicTypes["int"];
    if (!type->isIntegerTy()) {
      return error("cannot use a non-integral type for "
                   "enum values at line {}",
                   state_.row + 1);
    }
    if (options_.size() > Type::getBitSize(module, type)) {
      return error("type provided for enum `{}` has insufficient "
                   "size to hold the enum values at line {}",
                   name_, state_.row + 1);
    }
    for (size_t i = 0; i < options_.size(); ++i) {
      const auto option = format("{}::{}", name_, options_[i].data());
      if (module->getNamedValue(option)) {
        return error("value `{}` already exists at line {}", option,
                     state_.row + 1);
      }
      const auto value = llvm::cast<llvm::Constant>(Integral::get(i, type));
      constexpr static auto linkage = llvm::Function::ExternalLinkage;
      (void)llvm::GlobalVariable{*module, type, true, linkage, value, option};
    }
    // using EnumName = UnderlyingType;
    if (auto err = Alias::add(module, name_, type)) {
      return err;
    }
    return llvm::Error::success();
  }

  inline const auto& name() const { return name_; }

private:
  const mpc_state_t state_;
  const std::string name_;
  std::unique_ptr<Type> underlyingType_;
  ident_list_t options_;
};

class EnumerationStmt final : public Stmt {
public:
  explicit EnumerationStmt(const mpc_ast_t* const ast)
      : Stmt(kEnumeration), impl_{ast} {}

  inline llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    return impl_.codegen(builder.GetInsertBlock()->getModule());
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    // @todo Rename enumeration to anon name
    // auto MD = getMetadataOperand(*module, "aliases", impl_.name());
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kEnumeration;
  }

private:
  const Enumeration impl_;
};

} // end namespace whack::ast

#endif // WHACK_ENUMERATION_HPP
