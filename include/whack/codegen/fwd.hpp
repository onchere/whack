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
#ifndef WHACK_AST_H
#define WHACK_AST_H

#pragma once

#include "types/types.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Casting.h>
#include <mpc/mpc.h>
#include <variant>
#include <whack/error.hpp>
#include <whack/format.hpp>

namespace whack {
template <typename T> using small_vector = llvm::SmallVector<T, 10>;
namespace codegen {

#include "../generated/reserved.def"

inline static bool isReserved(const llvm::StringRef name) {
  return std::find(RESERVED.begin(), RESERVED.end(), name) != RESERVED.end();
}

inline static auto& getGlobalContext() noexcept {
  return *reinterpret_cast<llvm::LLVMContext*>(LLVMGetGlobalContext());
}

static auto getTags(const mpc_ast_t* const ast) {
  small_vector<llvm::StringRef> rules;
  llvm::StringRef{ast->tag}.split(rules, '|');
  return rules;
}

inline static auto getOutermostAstTag(const mpc_ast_t* const ast) {
  return llvm::StringRef{ast->tag}.take_until(
      [](const char c) { return c == '|'; });
}

static auto getInnermostAstTag(const mpc_ast_t* const ast) {
  llvm::StringRef tag{ast->tag};
  tag.consume_back(">");
  tag.consume_back("regex");
  tag.consume_back("|");
  if (const auto i = tag.find_last_of('|')) {
    return tag.drop_front(i + 1);
  }
  return tag;
}

using ident_list_t = small_vector<llvm::StringRef>;

// ast is guaranteed to outlive the return vector
static auto getIdentList(const mpc_ast_t* const ast) {
  ident_list_t identList;
  if (!ast->children_num) {
    identList.push_back(ast->contents);
  } else {
    for (auto i = 0; i < ast->children_num; i += 2) {
      identList.push_back(ast->children[i]->contents);
    }
  }
  return identList;
}

class AST {
public:
  virtual ~AST() noexcept {}
};

class Expression : public AST {
public:
  virtual llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const = 0;
};

using expr_t = std::unique_ptr<Expression>;

namespace expressions {

static expr_t getExpressionValue(const mpc_ast_t* const);

static small_vector<expr_t> getExprList(const mpc_ast_t* const);

static llvm::Expected<llvm::Value*>
getLoadedValue(llvm::IRBuilder<>&, llvm::Value* const,
               const bool allowExpansion = false);

static llvm::Expected<small_vector<llvm::Value*>>
getExprValues(llvm::IRBuilder<>&, const small_vector<expr_t>&,
              const bool allowExpansion = false);

namespace factors {

class Factor : public AST {
public:
  enum Kind {
    kExpression,
    kMatchExpr,
    kNeg,
    kNot,
    kClosure,
    kNewExpr,
    kFnSizeOf,
    kFnAlignOf,
    kFnOffsetOf,
    kFnCast,
    kExpansion,
    kPreOp,
    kValue,
    kMemberInitList,
    kInitList,
    kCharacter,
    kFloatingPt,
    kIntegral,
    kBinary,
    kOctal,
    kHexaDecimal,
    kBoolean,
    kString,
    kScopeRes,
    kDeref,
    kListComprehension,
    kNullPtr,
    kIdent,
    kComposite
  };

private:
  const Kind kind_;

public:
  inline explicit constexpr Factor(const Kind kind) noexcept : kind_{kind} {}

  Factor(Factor&&) = default;
  Factor& operator=(Factor&&) = default;
  Factor(const Factor&) = delete;
  Factor& operator=(const Factor&) = delete;

  inline constexpr Kind getKind() const noexcept { return kind_; }
  virtual llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const = 0;
};

static std::unique_ptr<Factor> getFactor(const mpc_ast_t* const);

const static std::string getIdentifierString(const mpc_ast_t* const);

} // namespace factors

} // namespace expressions

namespace stmts {
class Stmt : public AST {
public:
  enum Kind {
    kBody,
    kYield,
    kReturn,
    kDelete,
    kUnreachable,
    kBreak,
    kContinue,
    kDefer,
    kIf,
    kWhile,
    kFor,
    kSelect,
    kAlias,
    kStructure,
    kEnumeration,
    kDataClass,
    kMatch,
    kTypeSwitch,
    kDeclAssign,
    kLet,
    kAssign,
    kOpEq,
    kComment,
    kExpression
  };

private:
  const Kind kind_;

public:
  inline explicit constexpr Stmt(const Kind kind) noexcept : kind_{kind} {}

  Stmt(Stmt&&) = default;
  Stmt& operator=(Stmt&&) = default;
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;

  inline constexpr Kind getKind() const noexcept { return kind_; }
  virtual llvm::Error codegen(llvm::IRBuilder<>&) const = 0;
  virtual llvm::Error runScopeExit(llvm::IRBuilder<>&) const {
    return llvm::Error::success();
  }
};

static std::unique_ptr<Stmt> getStmt(const mpc_ast_t* const);

class Body;

} // end namespace stmts

namespace types {
class Type;
static llvm::Expected<llvm::Type*> getType(const mpc_ast_t* const,
                                           llvm::IRBuilder<>&);

using typelist_t = std::pair<small_vector<llvm::Type*>, bool>;

static llvm::Expected<typelist_t> getTypeList(const mpc_ast_t* const,
                                              llvm::IRBuilder<>&);
} // namespace types

static llvm::Function* changeFuncReturnType(llvm::Function* const,
                                            llvm::Type* const);

static llvm::Expected<llvm::Type*>
deduceFuncReturnType(const llvm::Function* const);

static llvm::Function* bindFirstFuncArgument(llvm::IRBuilder<>&,
                                             llvm::Function* const,
                                             llvm::Value*,
                                             llvm::FunctionType* const);

static llvm::Expected<llvm::Function*> buildFunction(llvm::Function*,
                                                     const stmts::Body* const);

using structopname_t = std::variant<llvm::StringRef, types::Type>;

static structopname_t getStructOpName(const mpc_ast_t* const);

static llvm::Expected<std::string> getStructOpNameString(llvm::IRBuilder<>&,
                                                         const structopname_t&);

struct MatchInfo {
  using match_res_t = std::variant<std::unique_ptr<stmts::Stmt>, expr_t>;
  llvm::Value* Subject;
  std::vector<std::pair<small_vector<llvm::Value*>, match_res_t>> Options;
  std::optional<match_res_t> Default;
  bool IsExpression;
};

static llvm::Expected<MatchInfo> getMatchInfo(const mpc_ast_t* const,
                                              llvm::IRBuilder<>&);

} // end namespace codegen
} // end namespace whack

#endif // WHACK_AST_H
