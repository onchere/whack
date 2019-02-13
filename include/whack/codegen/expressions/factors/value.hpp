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
#ifndef WHACK_VALUE_HPP
#define WHACK_VALUE_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Value final : public Factor {
public:
  explicit Value(const mpc_ast_t* const ast)
      : Factor(kValue), type_{ast->children[0]}, init_{ast->children[1]} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto tp = type_.codegen(builder);
    if (!tp) {
      return tp.takeError();
    }
    const auto type = *tp;
    const auto init = init_.list();
    switch (init.index()) {
    case 0: // <initlist>
      return std::get<InitList>(init).codegen(builder, type);
    default: // <memberinitlist>
      return std::get<MemberInitList>(init).codegen(builder, type);
    }
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kValue;
  }

private:
  const types::Type type_;
  const Initializer init_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_VALUE_HPP
