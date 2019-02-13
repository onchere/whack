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
#ifndef WHACK_EXPORTS_HPP
#define WHACK_EXPORTS_HPP

#pragma once

#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::codegen::elements {

class Exports final : public AST {
public:
  explicit Exports(const mpc_ast_t* const ast)
      : state_{ast->state}, exportSymbols_{getIdentList(ast->children[1])} {}

  llvm::Error codegen(llvm::Module* const module) const {
    const auto MD = module->getOrInsertNamedMetadata("exports");
    llvm::MDBuilder MDBuilder{module->getContext()};
    for (const auto& symbol : exportSymbols_) {
      if (getMetadataOperand(*module, "exports", symbol)) {
        return error("symbol `{}` is already exported in module "
                     "`{}` at line {}",
                     symbol.data(), module->getModuleIdentifier(),
                     state_.row + 1);
      }
      MD->addOperand(MDBuilder.createTBAARoot(symbol));
    }
    return llvm::Error::success();
  }

  inline const auto& get() const { return exportSymbols_; }

  inline static auto get(const llvm::Module* const module) {
    return getMetadataParts<1>(*module, "exports");
  }

private:
  const mpc_state_t state_;
  const ident_list_t exportSymbols_;
};

} // end namespace whack::codegen::elements

#endif // WHACK_EXPORTS_HPP
