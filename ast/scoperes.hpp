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
#ifndef WHACK_SCOPERES_HPP
#define WHACK_SCOPERES_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class ScopeRes final : public Factor {
public:
  explicit constexpr ScopeRes(const mpc_ast_t* const ast) noexcept
      : ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    if (ast_->children_num == 3) { // we likely have a enum value
      const auto& enumName = ast_->children[0]->contents;
      const auto& value = ast_->children[2]->contents;
      const auto name = format("{}::{}", enumName, value);
      if (auto value = module->getGlobalVariable(name)) {
        return value;
      }
    }
    llvm_unreachable("not implemented");
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace whack::ast

#endif // WHACK_SCOPERES_HPP
