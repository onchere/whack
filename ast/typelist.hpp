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
#ifndef WHACK_TYPELIST_HPP
#define WHACK_TYPELIST_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class TypeList final : public AST {
public:
  explicit constexpr TypeList(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  typelist_t codegen(const llvm::Module* const module) const {
    bool variadic = false;
    small_vector<llvm::Type*> types;
    const auto tag = getInnermostAstTag(ast_);
    if (tag == "variadictype") {
      types.push_back(getType(ast_->children[0], module));
      variadic = true;
    } else if (tag != "typelist" || !ast_->children_num) {
      types = {getType(ast_, module)};
    } else {
      for (auto i = 0; i < ast_->children_num; i += 2) {
        const auto ref = ast_->children[i];
        if (getInnermostAstTag(ref) == "variadictype") {
          types.push_back(getType(ref->children[0], module));
          variadic = true;
        } else {
          types.push_back(getType(ref, module));
        }
      }
    }
    return {std::move(types), variadic};
  }

private:
  const mpc_ast_t* const ast_;
};

inline static typelist_t getTypeList(const mpc_ast_t* const ast,
                                     const llvm::Module* const module) {
  return TypeList{ast}.codegen(module);
}

} // end namespace whack::ast

#endif // WHACK_TYPELIST_HPP
