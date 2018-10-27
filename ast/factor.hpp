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
#ifndef WHACK_FACTOR_HPP
#define WHACK_FACTOR_HPP

#pragma once

#include "boolean.hpp"
#include "character.hpp"
#include "closure.hpp"
#include "deref.hpp"
#include "element.hpp"
#include "expandop.hpp"
#include "expansion.hpp"
#include "floatingpt.hpp"
#include "fnalignof.hpp"
#include "fncast.hpp"
#include "fnlen.hpp"
#include "funccall.hpp"
#include "ident.hpp"
#include "integral.hpp"
#include "listcomprehension.hpp"
#include "newexpr.hpp"
#include "nullptr.hpp"
#include "postop.hpp"
#include "preop.hpp"
#include "scoperes.hpp"
#include "string.hpp"
#include "structmember.hpp"
#include "value.hpp"

namespace whack::ast {

static std::unique_ptr<Factor> getFactor(const mpc_ast_t* const ast) {
  const auto tag = getInnermostAstTag(ast);
#define OPT(RULE, CLASS)                                                       \
  if (tag == RULE) {                                                           \
    return std::make_unique<CLASS>(ast);                                       \
  }
  OPT("character", Character)
  OPT("boolean", Boolean)
  OPT("integral", Integral)
  OPT("floatingpt", FloatingPt)
  OPT("preop", PreOp)
  OPT("postop", PostOp)
  OPT("funccall", FuncCall)
  OPT("element", Element)
  OPT("expandop", ExpandOp)
  OPT("structmember", StructMember)
  OPT("closure", Closure)
  OPT("fncast", FnCast)
  OPT("value", Value)
  OPT("deref", Deref)
  OPT("newexpr", NewExpr)
  OPT("scoperes", ScopeRes)
  OPT("listcomprehension", ListComprehension)
#undef OPT
  if (tag == "ident") {
    if (std::string_view(ast->contents) == "nullptr") {
      return std::make_unique<NullPtr>();
    }
    return std::make_unique<Ident>(ast);
  }
  if (tag == "string") { // @todo
    const std::string_view view{ast->contents};
    if (view == "true" || view == "false") {
      return std::make_unique<Boolean>(ast);
    }
    if (view == "...") {
      return std::make_unique<Expansion>(ast);
    }
    return std::make_unique<String>(ast);
  }
  llvm_unreachable("invalid factor kind!");
}

} // end namespace whack::ast

#endif // WHACK_FACTOR_HPP
