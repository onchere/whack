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
#ifndef WHACK_STMT_HPP
#define WHACK_STMT_HPP

#pragma once

// #include "forstmt.hpp"
#include "../elements/alias.hpp"
#include "../elements/comment.hpp"
#include "../elements/dataclass.hpp"
#include "../elements/enumeration.hpp"
#include "../elements/structure.hpp"
#include "../expressions/expression.hpp"
#include "assign.hpp"
#include "body.hpp"
#include "breakstmt.hpp"
#include "continuestmt.hpp"
#include "declassign.hpp"
#include "deferstmt.hpp"
#include "deletestmt.hpp"
#include "ifstmt.hpp"
#include "let.hpp"
#include "match.hpp"
#include "opeq.hpp"
#include "returnstmt.hpp"
#include "typeswitch.hpp"
#include "unreachable.hpp"
#include "whilestmt.hpp"
#include "yieldstmt.hpp"

namespace whack::codegen {

namespace stmts {

using elements::AliasStmt;
using elements::CommentStmt;
using elements::DataClassStmt;
using elements::EnumerationStmt;
using elements::StructureStmt;
using expressions::factors::FuncCallStmt;
using expressions::factors::NewExprStmt;
using expressions::factors::PostOpStmt;
using expressions::factors::PreOpStmt;

static std::unique_ptr<Stmt> getStmt(const mpc_ast_t* const ast) {
  auto ref = ast;
  std::string tag;
  if (getInnermostAstTag(ref) == "stmt") { //
    ref = ref->children[0];
    tag = getInnermostAstTag(ref).str();
  } else {
    tag = getTags(ref)[1].str();
  }

#define OPT(RULE, CLASS)                                                       \
  if (tag == RULE) {                                                           \
    return std::make_unique<CLASS>(ref);                                       \
  }
  OPT("body", Body)
  OPT("alias", AliasStmt)
  OPT("let", Let)
  OPT("match", Match)
  OPT("unreachable", Unreachable)
  OPT("returnstmt", Return)
  OPT("declassign", DeclAssign)
  OPT("whilestmt", While)
  OPT("ifstmt", If)
  // OPT("forstmt", For)
  OPT("assign", Assign)
  OPT("funccall", FuncCallStmt)
  OPT("newexpr", NewExprStmt)
  OPT("preop", PreOpStmt)
  OPT("postop", PostOpStmt)
  OPT("typeswitch", TypeSwitch)
  OPT("opeq", OpEq)
  OPT("deletestmt", Delete)
  OPT("yieldstmt", YieldStmt)
  OPT("breakstmt", Break)
  OPT("continuestmt", Continue)
  OPT("structure", StructureStmt)
  OPT("enumeration", EnumerationStmt)
  OPT("dataclass", DataClassStmt)
  OPT("deferstmt", Defer)
  OPT("comment", CommentStmt)
  OPT("unreachable", Unreachable)
#undef OPT
  llvm_unreachable("invalid statement kind!");
}

} // end namespace stmts
} // end namespace whack::codegen

#endif // WHACK_STMT_HPP
