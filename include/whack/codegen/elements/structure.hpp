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
#ifndef WHACK_STRUCTURE_HPP
#define WHACK_STRUCTURE_HPP

#pragma once

#include "../stmts/declassign.hpp"
#include "../tags.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::codegen::elements {

class Structure final : public AST {
public:
  using member_t = std::pair<std::optional<Tags>, stmts::DeclAssign>;

  explicit Structure(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[0]->contents} {
    const auto def = ast->children[1];
    for (auto i = 2; i < def->children_num - 1; ++i) {
      const auto ref = def->children[i];
      if (std::string_view(ref->contents) == ";") {
        continue;
      }
      if (getInnermostAstTag(ref) == "tags") {
        members_.emplace_back(std::pair{std::optional{Tags{ref}},
                                        stmts::DeclAssign{def->children[++i]}});
      } else {
        members_.emplace_back(std::pair{std::nullopt, stmts::DeclAssign{ref}});
      }
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(module, name_)) {
      return err;
    }
    auto& ctx = module->getContext();
    auto structure = llvm::StructType::create(ctx, name_);
    small_vector<llvm::Type*> fields;
    small_vector<llvm::StringRef> fieldNames;
    llvm::IRBuilder<> builder{ctx};
    for (const auto& [tags, decl] : members_) {
      if (!decl.initializers().empty()) {
        warning("member initializers ignored in struct "
                "`{}` at line {} (tip: use constructor "
                "instead to assign values to fields)",
                name_, state_.row + 1);
      }
      // @todo Use tags
      auto type = decl.type(builder);
      if (!type) {
        return type.takeError();
      }
      for (const auto& var : decl.variables()) {
        fieldNames.push_back(var);
        fields.push_back(*type);
      }
    }
    addStructTypeMetadata(module, "structures", name_, fieldNames);
    structure->setBody(fields);
    return llvm::Error::success();
  }

  inline const auto& name() const { return name_; }

private:
  friend class StructureStmt;
  const mpc_state_t state_;
  const std::string name_;
  std::vector<member_t> members_;
};

class StructureStmt final : public stmts::Stmt {
public:
  explicit StructureStmt(const mpc_ast_t* const ast)
      : Stmt(kStructure), impl_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(builder, impl_.name_)) {
      return err;
    }
    return impl_.codegen(builder.GetInsertBlock()->getModule());
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    const auto structure = module->getTypeByName(impl_.name_);
    // We just "mangle" the name, we don't delete
    structure->setName(".tmp." + impl_.name_);
    renameMetadataOperand(*module, "structures", impl_.name_,
                          structure->getName());
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kStructure;
  }

private:
  const Structure impl_;
};

} // end namespace whack::codegen::elements

#endif // WHACK_STRUCTURE_HPP
