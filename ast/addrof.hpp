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
#ifndef WHACK_ADDROF_HPP
#define WHACK_ADDROF_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class AddrOf final : public Expression {
public:
  explicit AddrOf(const mpc_ast_t* const ast)
      : value_{getFactor(ast->children[1])} {}

  inline llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>& builder) const final {
    // @todo: tag value as an 'address'?
    return value_->codegen(builder);
  }

private:
  std::unique_ptr<Factor> value_;
};

} // end namespace whack::ast

#endif // WHACK_ADDROF_HPP
