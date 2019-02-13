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
#ifndef WHACK_NULLPTR_HPP
#define WHACK_NULLPTR_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class NullPtr final : public Factor {
public:
  constexpr NullPtr(const mpc_ast_t* const = nullptr) noexcept
      : Factor(kNullPtr) {}

  inline static llvm::Value*
  get(const llvm::Type* const type = BasicTypes["char"]) {
    return llvm::Constant::getNullValue(type->getPointerTo(0));
  }

  inline llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const final {
    return get();
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kNullPtr;
  }
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_NULLPTR_HPP
