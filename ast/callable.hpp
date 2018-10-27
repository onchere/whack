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
#ifndef WHACK_CALLABLE_HPP
#define WHACK_CALLABLE_HPP

#pragma once

#include "ast.hpp"
#include "closure.hpp"
#include "overloadid.hpp"
#include "scoperes.hpp"
#include "variable.hpp"

namespace whack::ast {

using callable_t = std::variant<Closure, OverloadID, ScopeRes, variable_t>;

const static callable_t getCallable(const mpc_ast_t* const ast) {
  const auto tag = getInnermostAstTag(ast);
  if (tag == "closure") {
    return Closure{ast};
  }
  if (tag == "overloadid") {
    return OverloadID{ast};
  }
  if (tag == "scoperes") {
    return ScopeRes{ast};
  }
  if (tag == "variable") {
    return getVariable(ast);
  }
  llvm_unreachable("unknown callable kind!");
}

} // end namespace whack::ast

#endif // WHACK_CALLABLE_HPP
