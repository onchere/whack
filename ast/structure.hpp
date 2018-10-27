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
#ifndef WHACK_STRUCTURE_HPP
#define WHACK_STRUCTURE_HPP

#pragma once

#include "declassign.hpp"
#include "tags.hpp"
#include <llvm/IR/MDBuilder.h>

namespace whack::ast {

class Structure final : public AST {
public:
  using member_t = std::pair<std::optional<Tags>, DeclAssign>;

  explicit Structure(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[1]->contents} {
    const auto& def = ast->children[2];
    for (auto i = 2; i < def->children_num - 1; ++i) {
      const auto& ref = def->children[i];
      if (getInnermostAstTag(ref) == "tags") {
        members_.emplace_back(std::pair{std::optional{Tags{ref}},
                                        DeclAssign{def->children[++i]}});
      } else {
        members_.emplace_back(std::pair{std::nullopt, DeclAssign{ref}});
      }
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    auto structure = llvm::StructType::create(module->getContext(), name_);
    llvm::SmallVector<llvm::Type*, 10> fields;
    llvm::SmallVector<llvm::StringRef, 10> fieldNames;
    for (const auto& [tags, decl] : members_) {
      if (!decl.initializers().empty()) {
        warning("member initializers ignored in struct "
                "`{}` at line {} (tip: use constructor "
                "instead to assign values to fields)",
                name_, state_.row + 1);
      }
      // @todo Use tags
      auto type = decl.type(module);
      if (llvm::isa<llvm::FunctionType>(type)) {
        type = type->getPointerTo(0); // we store function pointers
      }
      for (const auto& var : decl.variables()) {
        fieldNames.push_back(var);
        fields.push_back(type);
      }
    }
    addMetadata(module, name_, fieldNames);
    structure->setBody(fields);
    return llvm::Error::success();
  }

  const auto& name() const { return name_; }

  static void addMetadata(llvm::Module* const module, llvm::StringRef name,
                          llvm::ArrayRef<llvm::StringRef> fields) {
    std::vector<std::pair<llvm::MDNode*, uint64_t>> metadata;
    llvm::MDBuilder MDBuilder{module->getContext()};
    for (const auto& field : fields) {
      const auto MD =
          reinterpret_cast<llvm::MDNode*>(MDBuilder.createString(field));
      metadata.emplace_back(std::pair{MD, metadata.size()});
    }
    const auto structMD = MDBuilder.createTBAAStructTypeNode(name, metadata);
    module->getOrInsertNamedMetadata("structures")->addOperand(structMD);
  }

private:
  const mpc_state_t state_;
  const std::string name_;
  std::vector<member_t> members_;
};

class StructureStmt final : public Stmt {
public:
  explicit StructureStmt(const mpc_ast_t* const ast)
      : Stmt(kStructure), impl_{ast} {}

  inline llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    return impl_.codegen(builder.GetInsertBlock()->getModule());
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    // // We mess the name, so those out of this scope don't access type
    // const auto structrure = <get from this function's value symbol table>
    // structure->setName(".tmp." + name);
    // const auto &module = *builder.GetInsertBlock()->getParent()->getParent();
    // if(const auto MD = getMetadataOperand(module, "structures", name)){
    //     auto node = MD.value();
    //     // @todo Rename MD to ".tmp." + name
    // }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kStructure;
  }

private:
  const Structure impl_;
  mutable llvm::StructType* structure_;
};

} // end namespace whack::ast

#endif // WHACK_STRUCTURE_HPP
