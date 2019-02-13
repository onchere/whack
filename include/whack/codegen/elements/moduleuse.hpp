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
#ifndef WHACK_MODULEUSE_HPP
#define WHACK_MODULEUSE_HPP

#pragma once

#include "../expressions/factors/identifier.hpp"
#include "../stmts/declassign.hpp"
#include <llvm/IR/MDBuilder.h>
#include <llvm/Support/raw_ostream.h>

namespace whack::codegen {

struct ModuleImportInfo {
  llvm::StringRef modulePath;
  llvm::StringRef moduleName;
  std::optional<llvm::StringRef> qualifier;
  std::optional<ident_list_t> elements;
  bool hiding;
};

static llvm::Error importModule(llvm::Module* const dest,
                                const ModuleImportInfo);

namespace elements {

class ModuleUse final : public AST {
public:
  explicit ModuleUse(const mpc_ast_t* const ast)
      : state_{ast->state}, identifier_{expressions::factors::getIdentifier(
                                ast->children[1])} {
    const auto num = ast->children_num;
    if (num > 2) {
      const std::string_view view{ast->children[2]->contents};
      if (view == "!") {
        elements_ = getIdentList(ast->children[4]);
        hiding_ = true;
      } else if (view == "{") {
        elements_ = getIdentList(ast->children[3]);
      }
      if (num > 4) {
        if (std::string_view(ast->children[num - 3]->contents) == "as") {
          as_ = ast->children[num - 2]->contents;
        }
      }
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    using namespace expressions::factors;
    std::string modulePath, moduleName;
    switch (identifier_.index()) {
    case 1: // <scoperes>
    {
      const auto& name = std::get<ScopeRes>(identifier_).name();
      llvm::SmallVector<llvm::StringRef, 5 /*arbitrary*/> paths;
      llvm::StringRef{name}.split(paths, "::");
      moduleName = paths.back().str();
      llvm::raw_string_ostream os{modulePath};
      for (size_t i = 0; i < paths.size(); ++i) {
        os << paths[i];
        if (i < paths.size() - 1) {
          os << '/';
        }
      }
      os.flush();
      break;
    }
    case 2: // <ident>
    {
      const auto& name = std::get<Ident>(identifier_).name();
      modulePath = moduleName = name;
      break;
    }
    default:
      llvm_unreachable("invalid identifier kind for module use!");
    }
    // Let's help the (yet to be created, qualified) typename resolver
    if (as_) {
      const auto MD = module->getOrInsertNamedMetadata("qualifiers");
      llvm::MDBuilder MDBuilder{module->getContext()};
      MD->addOperand(MDBuilder.createTBAARoot(as_.value()));
    }
    return importModule(module,
                        {modulePath, moduleName, as_, elements_, hiding_});
  }

private:
  const mpc_state_t state_;
  expressions::factors::identifier_t identifier_;
  std::optional<ident_list_t> elements_;
  bool hiding_{false};
  std::optional<llvm::StringRef> as_;
};

} // end namespace elements
} // end namespace whack::codegen

#endif // WHACK_MODULEUSE_HPP
