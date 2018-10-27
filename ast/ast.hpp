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
#ifndef WHACK_AST_H
#define WHACK_AST_H

#pragma once

#include "../error.hpp"
#include "../format.hpp"
#include "../mpc/mpc.h"
#include "../types.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Casting.h>

namespace whack {
template <typename T> using small_vector = llvm::SmallVector<T, 10>;
namespace ast {

#include "../reserved.def"

inline static bool isReserved(llvm::StringRef name) {
  return std::find(RESERVED.begin(), RESERVED.end(), name) != RESERVED.end();
}

static auto getTags(const mpc_ast_t* const ast) {
  small_vector<std::string> tags;
  std::string tag;
  for (const auto chr : std::string_view(ast->tag)) {
    if (chr == '|') {
      tags.push_back(tag);
      tag.clear();
    } else {
      tag.push_back(chr);
    }
  }
  tags.emplace_back(std::move(tag));
  return tags;
}

inline static auto getOutermostAstTag(const mpc_ast_t* const ast) {
  return getTags(ast).front();
}

static auto getInnermostAstTag(const mpc_ast_t* const ast) {
  const auto tags = getTags(ast);
  const auto& back = tags.back();
  if (back == "regex" || back == ">") {
    return tags[tags.size() - 2];
  }
  return std::move(back);
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

static expr_t getExpressionValue(const mpc_ast_t* const);

static small_vector<expr_t> getExprList(const mpc_ast_t* const);

class Factor : public AST {
public:
  enum Kind {
    kExpression,
    kClosure,
    kNewExpr,
    kFnSizeOf,
    kFnAlignOf,
    kFnAppend,
    kFnLen,
    kFnCast,
    kFuncCall,
    kReceive,
    kExpansion,
    kExpandOp,
    kPreOp,
    kPostOp,
    kValue,
    kInitializer,
    kCharacter,
    kFloatingPt,
    kIntegral,
    kBoolean,
    kString,
    kElement,
    kStructMember,
    kScopeRes,
    kReference,
    kDeref,
    kRange
  };

private:
  const Kind kind_;

public:
  inline explicit constexpr Factor(const Kind kind = kExpression) noexcept
      : kind_{kind} {}

  // Factor(const Factor&) = delete;
  // Factor& operator=(const Factor&) = delete;

  inline const Kind getKind() const noexcept { return kind_; }
  virtual llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>&) const = 0;
};

static std::unique_ptr<Factor> getFactor(const mpc_ast_t* const);

class Stmt : public AST {
public:
  enum Kind {
    kBody,
    kYield,
    kReturn,
    kCoReturn,
    kDelete,
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
    kMatch,
    kTypeSwitch,
    kDeclAssign,
    kLetExpr,
    kAssign,
    kOpEq,
    kFuncCall,
    kSend,
    kReceive,
    kOutStream,
    kInStream,
    kPreOp,
    kPostOp
  };

private:
  const Kind kind_;

public:
  inline explicit constexpr Stmt(const Kind kind) noexcept : kind_{kind} {}

  // Stmt(const Stmt&) = delete;
  // Stmt& operator=(const Stmt&) = delete;

  inline const Kind getKind() const noexcept { return kind_; }
  virtual llvm::Error codegen(llvm::IRBuilder<>&) const = 0;
  virtual llvm::Error runScopeExit(llvm::IRBuilder<>&) const {
    return llvm::Error::success();
  }
};

static std::unique_ptr<Stmt> getStmt(const mpc_ast_t* const);

static llvm::Type* getType(const mpc_ast_t* const, const llvm::Module* const);

using typelist_t = std::pair<small_vector<llvm::Type*>, bool>;

typelist_t static getTypeList(const mpc_ast_t* const,
                              const llvm::Module* const);

static llvm::Function* changeFuncReturnType(llvm::Function* const,
                                            llvm::Type* const);

static llvm::Expected<llvm::Type*>
deduceFuncReturnType(const llvm::Function* const, const mpc_state_t);

static llvm::Function* bindFirstFuncArgument(llvm::IRBuilder<>&,
                                             llvm::Function* const,
                                             llvm::Value* const,
                                             llvm::FunctionType* const);

inline static /*const*/ llvm::StringMap<decltype(&LLVMBuildAnd)> OpsTable{
    {"&", &LLVMBuildAnd},   {"|", &LLVMBuildOr},     {"+", &LLVMBuildNSWAdd},
    {"+f", &LLVMBuildFAdd}, {"-", &LLVMBuildNSWSub}, {"-f", &LLVMBuildFSub},
    {"^", &LLVMBuildXor},   {"%", &LLVMBuildFRem},   {"/", &LLVMBuildSDiv},
    {"/f", &LLVMBuildFDiv}, {"*", &LLVMBuildNSWMul}, {"*f", &LLVMBuildFMul},
    {">>", &LLVMBuildAShr}, {"<<", &LLVMBuildShl}};

} // end namespace ast
} // end namespace whack

#endif // WHACK_AST_H
