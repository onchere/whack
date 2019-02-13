/**
 * Copyright 2019-present Onchere Bironga
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
#ifndef WHACK_NOT_HPP
#define WHACK_NOT_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Not final : public Factor {
public:
  explicit Not(const mpc_ast_t* const ast)
      : Factor(kNot), state_{ast->state}, factor_{getFactor(ast)} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto factor = factor_->codegen(builder);
    if (!factor) {
      return factor.takeError();
    }
    if (!(*factor)->getType()->isIntegerTy(1)) {
      return error("not operator not implemented for type at line {}",
                   state_.row + 1);
    }
    return builder.CreateNot(*factor);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kNot;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<Factor> factor_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_NOT_HPP
