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
#ifndef WHACK_REFERENCE_HPP
#define WHACK_REFERENCE_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Reference final : public Factor {
public:
  explicit Reference(const mpc_ast_t* const ast)
      : Factor(kReference), variable_{getFactor(ast->children[0])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto variable = variable_->codegen(builder);
    if (!variable) {
      return variable.takeError();
    }
    const auto ref =
        builder.CreateAlloca((*variable)->getType(), 0, nullptr, "");
    setIsDereferenceable(builder.getContext(), ref);
    builder.CreateStore(*variable, ref);
    return ref;
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kReference;
  }

private:
  const std::unique_ptr<Factor> variable_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_REFERENCE_HPP
