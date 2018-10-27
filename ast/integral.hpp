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
#ifndef WHACK_INTEGRAL_HPP
#define WHACK_INTEGRAL_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class Integral final : public Factor {
public:
  explicit Integral(const mpc_ast_t* const ast)
      : integral_{static_cast<std::int64_t>(std::atoi(ast->contents))} {}

  inline static llvm::Value* zero(llvm::Type* const type = BasicTypes["int"]) {
    return get(0, type);
  }

  inline static llvm::Value* one(llvm::Type* const type = BasicTypes["int"]) {
    return get(1, type);
  }

  inline static llvm::Value* get(const int value,
                                 llvm::Type* const type = BasicTypes["int"]) {
    return llvm::ConstantInt::get(type, value);
  }

  inline llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>&) const final { // @todo Proper type
    return llvm::ConstantInt::get(BasicTypes["int"], integral_);
  }

  inline const auto value() const noexcept { return integral_; }

private:
  const std::int64_t integral_;
};

} // end namespace whack::ast

#endif // WHACK_INTEGRAL_HPP
