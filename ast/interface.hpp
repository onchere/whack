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
#ifndef WHACK_INTERFACE_HPP
#define WHACK_INTERFACE_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Interface final : public AST {
public:
  explicit Interface(const mpc_ast_t* const ast)
      : name_{ast->children[1]->contents} {
    const auto& ref = ast->children[2];
    auto idx = 1;
    if (std::string_view(ref->children[idx]->contents) == ":") {
      for (idx = 2; idx < ref->children_num; ++idx) {
        const auto inherit = ref->children[idx];
        const std::string_view sv{inherit->contents};
        if (sv == ",") {
          continue;
        }
        if (sv == "{") {
          ++idx;
          break;
        }
        // @todo Inherit
      }
    }
    for (++idx; idx < ref->children_num - 1; idx += 2) {
      Type type{ref->children[idx]};
      if (getInnermostAstTag(ref->children[idx + 1]) == "ident") {
        std::string name{ref->children[idx + 1]->contents};
        functions_.emplace_back(std::pair{std::move(type), std::move(name)});
        ++idx;
      } else {
        functions_.emplace_back(std::pair{std::move(type), std::nullopt});
      }
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    auto& ctx = module->getContext();
    const auto impl = llvm::StructType::create(ctx, "interface::" + name_);
    std::vector<llvm::Type*> funcs;
    std::vector<std::pair<llvm::MDNode*, uint64_t>> metadata;
    llvm::MDBuilder MDBuilder{ctx};
    auto idx = 0;
    for (const auto& [type, name] : functions_) {
      funcs.push_back(type.codegen(module)->getPointerTo(0));
      llvm::MDString* nameMD;
      if (name) {
        nameMD = MDBuilder.createString(name.value());
      } else {
        nameMD = MDBuilder.createString(std::to_string(idx++));
      }
      metadata.emplace_back(
          std::pair{reinterpret_cast<llvm::MDNode*>(nameMD), funcs.size()});
    }
    impl->setBody(funcs);
    const auto interfaceMD =
        MDBuilder.createTBAAStructTypeNode(name_, metadata);
    module->getOrInsertNamedMetadata("interfaces")->addOperand(interfaceMD);
    return llvm::Error::success();
  }

  const auto& name() const { return name_; }
  const auto& functions() const { return functions_; }

private:
  const std::string name_;
  std::vector<std::string> inherits_;
  using name_t = std::optional<std::string>;
  small_vector<std::pair<Type, name_t>> functions_;
};

} // end namespace whack::ast

#endif // WHACK_INTERFACE_HPP
