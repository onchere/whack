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
#ifndef WHACK_FNALIGNOF_HPP
#define WHACK_FNALIGNOF_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class FnAlignOf final : public Factor {
public:
  explicit constexpr FnAlignOf(const mpc_ast_t* const ast) noexcept
      : Factor(kFnAlignOf), type_{ast->children[2]} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto type = type_.codegen(builder);
    if (!type) {
      return type.takeError();
    }
    return llvm::ConstantExpr::getAlignOf(*type);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kFnAlignOf;
  }

private:
  const types::Type type_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_FNALIGNOF_HPP
