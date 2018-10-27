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
#ifndef WHACK_STMT_HPP
#define WHACK_STMT_HPP

#pragma once

#include "alias.hpp"
#include "assign.hpp"
#include "body.hpp"
#include "breakstmt.hpp"
#include "continuestmt.hpp"
#include "coreturnstmt.hpp"
#include "declassign.hpp"
#include "deferstmt.hpp"
#include "deletestmt.hpp"
#include "enumeration.hpp"
#include "forstmt.hpp"
#include "funccall.hpp"
#include "ifstmt.hpp"
#include "instream.hpp"
#include "letexpr.hpp"
#include "match.hpp"
#include "opeq.hpp"
#include "outstream.hpp"
#include "postop.hpp"
#include "preop.hpp"
#include "receive.hpp"
#include "returnstmt.hpp"
#include "select.hpp"
#include "send.hpp"
#include "structure.hpp"
#include "typeswitch.hpp"
#include "whilestmt.hpp"
#include "yieldstmt.hpp"

namespace whack::ast {

static std::unique_ptr<Stmt> getStmt(const mpc_ast_t* const ast) {
  auto ref = ast;
  if (getInnermostAstTag(ref) == "stmt") {
    ref = ref->children[0];
  }
  const auto tag = getInnermostAstTag(ref);

#define OPT(RULE, CLASS)                                                       \
  if (tag == RULE) {                                                           \
    return std::make_unique<CLASS>(ref);                                       \
  }
  OPT("body", Body)
  OPT("returnstmt", Return)
  OPT("declassign", DeclAssign)
  OPT("letexpr", LetExpr)
  OPT("whilestmt", While)
  OPT("ifstmt", If)
  OPT("forstmt", For)
  OPT("assign", Assign)
  OPT("funccall", FuncCallStmt)
  OPT("preop", PreOpStmt)
  OPT("postop", PostOpStmt)
  OPT("match", Match)
  OPT("typeswitch", TypeSwitch)
  OPT("opeq", OpEq)
  OPT("deletestmt", Delete)
  OPT("yieldstmt", YieldStmt)
  OPT("coreturnstmt", CoReturn)
  OPT("breakstmt", Break)
  OPT("continuestmt", Continue)
  OPT("select", Select)
  OPT("alias", AliasStmt)
  OPT("structure", StructureStmt)
  OPT("enumeration", EnumerationStmt)
  OPT("send", Send)
  OPT("receive", Receive)
  OPT("instream", InStream)
  OPT("outstream", OutStream)
  OPT("deferstmt", Defer)
#undef OPT
  llvm_unreachable("invalid statement kind!");
}

} // end namespace whack::ast

#endif // WHACK_STMT_HPP
