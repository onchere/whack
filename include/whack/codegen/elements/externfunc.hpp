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
#ifndef WHACK_EXTERNFUNC_HPP
#define WHACK_EXTERNFUNC_HPP

#pragma once

#include "../expressions/factors/ident.hpp"
#include "../types/fntype.hpp"

namespace whack::codegen::elements {

class ExternFunc final : public AST {
public:
  explicit ExternFunc(const mpc_ast_t* const ast)
      : type_{ast->children[1]}, name_{ast->children[2]->contents} {}

  llvm::Error codegen(llvm::Module* const module) const {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(module, name_)) {
      return err;
    }
    llvm::IRBuilder<> builder{module->getContext()};
    auto type = type_.codegen(builder);
    if (!type) {
      return type.takeError();
    }
    (void)llvm::Function::Create(*type, llvm::Function::ExternalLinkage, name_,
                                 module);
    return llvm::Error::success();
  }

private:
  const types::FnType type_;
  const llvm::StringRef name_;
};

} // end namespace whack::codegen::elements

#endif // WHACK_EXTERNFUNC_HPP
