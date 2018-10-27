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
#ifndef WHACK_CHARACTER_HPP
#define WHACK_CHARACTER_HPP

#pragma once

#include "ast.hpp"
#include <llvm/IR/Constants.h>

namespace whack::ast {

class Character final : public Factor {
public:
  explicit constexpr Character(const mpc_ast_t* const ast)
      : character_{ast->contents[1]} {} // @todo: Espaced stuff

  inline static auto get(const char character,
                         llvm::Type* const type = BasicTypes["char"]) {
    return llvm::ConstantInt::get(BasicTypes["char"],
                                  static_cast<uint64_t>(character));
  }

  inline llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const final {
    return llvm::ConstantInt::get(BasicTypes["char"],
                                  static_cast<uint64_t>(character_)); // @todo
  }

private:
  const char character_;
};

} // end namespace whack::ast

#endif // WHACK_CHARACTER_HPP
