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
#ifndef WHACK_NEG_HPP
#define WHACK_NEG_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Neg final : public Factor {
public:
  explicit Neg(const mpc_ast_t* const ast)
      : Factor(kNeg), state_{ast->state}, factor_{getFactor(ast)} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto factor = factor_->codegen(builder);
    if (!factor) {
      return factor.takeError();
    }
    const auto type = (*factor)->getType();
    if (type->isIntegerTy()) {
      return builder.CreateNeg(*factor);
    }
    if (type->isFloatingPointTy()) {
      return builder.CreateFNeg(*factor);
    }
    return error("negation operator not implemented for type at line {}",
                 state_.row + 1);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kNeg;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<Factor> factor_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_NEG_HPP
