/**
 * Copyright 2019-present Onchere Bironga
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
#ifndef WHACK_EXPRTYPE_HPP
#define WHACK_EXPRTYPE_HPP

#pragma once

#include "../fwd.hpp"
#include <lib/IR/LLVMContextImpl.h>

namespace whack::codegen::types {

class ExprType final : public AST {
public:
  explicit ExprType(const mpc_ast_t* const ast)
      : expr_{expressions::getExpressionValue(ast->children[2])} {}

  llvm::Expected<llvm::Type*> codegen(llvm::IRBuilder<>& builder) const {
    llvm::Module* module;
    if (const auto block = builder.GetInsertBlock()) {
      module = block->getModule();
    } else {
      module = *builder.getContext().pImpl->OwnedModules.begin();
    }
    llvm::Function* func;
    bool tempFunction = false;
    if (const auto block = builder.GetInsertBlock()) {
      func = block->getParent();
    } else {
      func = llvm::Function::Create(
          llvm::FunctionType::get(BasicTypes["void"], false),
          llvm::Function::ExternalLinkage, "", module);
      tempFunction = true;
    }
    const auto tempBlock =
        llvm::BasicBlock::Create(func->getContext(), "", func);
    llvm::IRBuilder<> tmp{tempBlock};
    auto e = expr_->codegen(tmp);
    if (!e) {
      return e.takeError();
    }
    auto expr = expressions::getLoadedValue(tmp, *e);
    if (!expr) {
      return expr.takeError();
    }
    if (tempFunction) {
      func->eraseFromParent();
    } else {
      tempBlock->eraseFromParent();
    }
    return (*expr)->getType();
  }

private:
  const expr_t expr_;
};

} // end namespace whack::codegen::types

#endif // WHACK_EXPRTYPE_HPP
