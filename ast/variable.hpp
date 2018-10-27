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
#ifndef WHACK_VARIABLE_HPP
#define WHACK_VARIABLE_HPP

#pragma once

#include "ast.hpp"
#include "element.hpp"
#include "ident.hpp"
#include "structmember.hpp"

namespace whack::ast {

using variable_t = std::variant<Element, StructMember, Ident>;

const static variable_t getVariable(const mpc_ast_t* const ast) {
  const auto tag = getInnermostAstTag(ast);
  if (tag == "element") {
    return Element{ast};
  }
  if (tag == "structmember") {
    return StructMember{ast};
  }
  if (tag == "ident") {
    return Ident{ast};
  }
  llvm_unreachable("invalid variable");
}

} // end namespace whack::ast

#endif // WHACK_VARIABLE_HPP
