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
#ifndef WHACK_FLOATINGPT_HPP
#define WHACK_FLOATINGPT_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class FloatingPt final : public Factor {
public:
  explicit FloatingPt(const mpc_ast_t* const ast)
      : floatingpt_{ast->contents} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const final {
    if (floatingpt_.back() == 'f') { // @todo: Refactor
      return llvm::ConstantFP::get(
          BasicTypes["float"], floatingpt_.substr(0, floatingpt_.size() - 2));
    }
    return llvm::ConstantFP::get(BasicTypes["double"], floatingpt_);
  }

private:
  const std::string floatingpt_;
};

} // end namespace whack::ast

#endif // WHACK_FLOATINGPT_HPP
