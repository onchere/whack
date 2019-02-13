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
#ifndef WHACK_HEXADECIMAL_HPP
#define WHACK_HEXADECIMAL_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class HexaDecimal final : public Factor {
public:
  explicit HexaDecimal(const mpc_ast_t* const ast) noexcept
      : Factor(kHexaDecimal), num_{std::strtol(ast->children[1]->contents,
                                               nullptr, 16)} {}

  inline llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const final {
    return Integral::get(num_);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kHexaDecimal;
  }

private:
  const int num_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_HEXADECIMAL_HPP
