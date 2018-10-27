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
#ifndef WHACK_EXTERNFUNC_HPP
#define WHACK_EXTERNFUNC_HPP

#pragma once

#include "type.hpp"
#include "typelist.hpp"

namespace whack::ast {

class ExternFunc final : public AST {
public:
  explicit ExternFunc(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[2]->contents} {
    if (getOutermostAstTag(ast->children[4]) == "typelist") {
      params_ = std::make_unique<TypeList>(ast->children[4]);
    }
    const auto ret = ast->children[ast->children_num - 2];
    if (getOutermostAstTag(ret) == "typelist") {
      returns_ = std::make_unique<TypeList>(ret);
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    auto t = this->type(module);
    if (!t) {
      return t.takeError();
    }
    constexpr static auto linkage =
        llvm::Function::LinkageTypes::ExternalLinkage;
    (void)llvm::Function::Create(*t, linkage, name_, module);
    return llvm::Error::success();
  }

private:
  const mpc_state_t state_;
  const std::string name_;
  std::unique_ptr<TypeList> params_;
  std::unique_ptr<TypeList> returns_;

  llvm::Expected<llvm::FunctionType*>
  type(const llvm::Module* const module) const {
    auto returnType = Type::getReturnType(
        module->getContext(),
        returns_ ? std::optional{returns_->codegen(module)} : std::nullopt,
        state_);
    if (!returnType) {
      return returnType.takeError();
    }
    if (params_) {
      const auto [types, variadic] = params_->codegen(module);
      return llvm::FunctionType::get(*returnType, types, variadic);
    }
    return llvm::FunctionType::get(*returnType, false);
  }
};

} // end namespace whack::ast

#endif // WHACK_EXTERNFUNC_HPP
