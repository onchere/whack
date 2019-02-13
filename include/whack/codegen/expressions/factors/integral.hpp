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
#ifndef WHACK_INTEGRAL_HPP
#define WHACK_INTEGRAL_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Integral final : public Factor {
public:
  explicit Integral(const mpc_ast_t* const ast)
      : Factor(kIntegral), integral_{static_cast<std::int64_t>(
                               std::atoi(ast->contents))} {}

  template <typename T = llvm::Value>
  inline static auto get(const int value,
                         llvm::Type* const type = BasicTypes["int"]) {
    return llvm::dyn_cast<T>(llvm::ConstantInt::get(type, value));
  }

  inline llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const final {
    return get(integral_);
  }

  inline const auto value() const noexcept { return integral_; }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kIntegral;
  }

private:
  const std::int64_t integral_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_INTEGRAL_HPP
