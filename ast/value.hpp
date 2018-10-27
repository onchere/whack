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
#ifndef WHACK_VALUE_HPP
#define WHACK_VALUE_HPP

#pragma once

#include "ast.hpp"
#include "initializer.hpp"
#include "type.hpp"

namespace whack::ast {

class Value final : public Factor {
public:
  explicit Value(const mpc_ast_t* const ast)
      : state_{ast->state}, type_{ast->children[0]}, init_{ast->children[1]} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    const auto type = type_.codegen(module);
    const auto init = init_.list();
    switch (init.index()) {
    case 0: // <initlist>
      return std::get<InitList>(init).codegen(builder, type, state_);
    case 1: // <listcomprehension>
      return std::get<ListComprehension>(init).codegen(builder, type, state_);
    default: // <memberinitlist>
      return std::get<MemberInitList>(init).codegen(builder, type, state_);
    }
  }

private:
  const mpc_state_t state_;
  const Type type_;
  const Initializer init_;
};

} // end namespace whack::ast

#endif // WHACK_VALUE_HPP
