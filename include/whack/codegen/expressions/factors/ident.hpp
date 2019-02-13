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
#ifndef WHACK_IDENT_HPP
#define WHACK_IDENT_HPP

#pragma once

#include "../../fwd.hpp"
#include "../../metadata.hpp"
#include "structmember.hpp"
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/Support/Error.h>

namespace whack::codegen::expressions::factors {

class Ident final : public Factor {
public:
  explicit Ident(const mpc_ast_t* const ast)
      : Factor(kIdent), state_{ast->state}, name_{ast->contents} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto func = builder.GetInsertBlock()->getParent();
    // variable
    if (auto var = func->getValueSymbolTable()->lookup(name_)) {
      return var;
    }

    // captured variables in closure
    const auto module = func->getParent();
    if (func->getName().startswith("::closure")) {
      const auto env = func->getValueSymbolTable()->lookup(".env");
      const auto structure = env->getType()->getPointerElementType();
      const auto idx =
          StructMember::getIndex(*module, structure->getStructName(), name_);
      if (idx) {
        const auto ptr =
            builder.CreateStructGEP(structure, env, idx.value(), name_);
        return builder.CreateLoad(ptr);
      }
    }

    // free function
    if (auto func = module->getFunction(name_)) {
      return func;
    }

    if (name_ == "_") {
      llvm::GlobalVariable* discard;
      if (discard = module->getGlobalVariable("_"); !discard) {
        static constexpr auto linkage = llvm::Function::InternalLinkage;
        discard = new llvm::GlobalVariable{BasicTypes["char"], true, linkage,
                                           nullptr, "_"};
        module->getGlobalList().push_back(discard);
      }
      return llvm::cast<llvm::Value>(discard);
    }

    return error("variable `{}` does not exist in "
                 "scope at line {}",
                 name_.data(), state_.row + 1);
  }

  inline const auto& name() const { return name_; }

  static llvm::Error isUnique(const llvm::Module* const module,
                              const llvm::StringRef name) {
    const auto isa = [&](auto&& kind) -> llvm::Error {
      return error("identifier `{}` already exists as a {} kind ", name.data(),
                   std::forward<decltype(kind)>(kind));
    };

    if (module->getValueSymbolTable().lookup(name)) {
      if (module->getNamedAlias(name)) {
        return isa("alias");
      }
      if (module->getFunction(name)) {
        return isa("alias");
      }
      return isa("global");
    }

    constexpr static auto KindsWithMD = {"structures", "interfaces", "classes"};
    for (const auto& kind : KindsWithMD) {
      if (getMetadataOperand(*module, kind, name)) {
        return isa(kind);
      }
    }
    return llvm::Error::success();
  }

  static llvm::Error isUnique(const llvm::IRBuilder<>& builder,
                              const llvm::StringRef name) {
    if (name == "_") {
      return error("identifier `{}` is reserved for discarded "
                   "assignment values");
    }

    if (isReserved(name)) {
      return error("identifier `{}` is reserved", name.data());
    }

    const auto func = builder.GetInsertBlock()->getParent();
    if (func->getValueSymbolTable()->lookup(name)) {
      return error("identifier `{}` already exists in function `{}` ",
                   name.data(), func->getName().str());
    }

    return isUnique(func->getParent(), name);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kIdent;
  }

private:
  const mpc_state_t state_;
  const std::string name_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_IDENT_HPP
