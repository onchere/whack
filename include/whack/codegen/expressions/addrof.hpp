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
#ifndef WHACK_ADDROF_HPP
#define WHACK_ADDROF_HPP

#pragma once

namespace whack::codegen::expressions {

class AddressOf final : public Expression {
public:
  explicit AddressOf(const mpc_ast_t* const ast)
      : state_{ast->state}, variable_{factors::getFactor(ast->children[1])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto var = variable_->codegen(builder);
    if (!var) {
      return var.takeError();
    }
    const auto val = *var;
    const auto ret = builder.CreateAlloca(val->getType(), 0, nullptr, "");
    builder.CreateStore(val, ret);
    return ret;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<factors::Factor> variable_;
};

} // end namespace whack::codegen::expressions

#endif // WHACK_ADDROF_HPP
