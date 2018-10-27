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
#ifndef WHACK_FUNCCALL_HPP
#define WHACK_FUNCCALL_HPP

#pragma once

#include "ast.hpp"
#include "dataclass.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

class FuncCall final : public Factor {
public:
  explicit constexpr FuncCall(const mpc_ast_t* const ast)
      : await_{std::string_view(ast->children[0]->contents) == "await"},
        async_{!await_ &&
               std::string_view(ast->children[0]->contents) == "async"},
        ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    // we store any value construction/call failures (FINAE)
    if (!await_ && !async_ &&
        getInnermostAstTag(ast_->children[0]) == "scoperes") {
      // LIKELY to be a data class in this module, not really a function call
      // @todo Refactor
      if (ast_->children[0]->children_num == 3) {
        if (auto val = DataClass::construct(ast_, builder)) {
          return *val;
        }
      }
      // @todo Refactor
      // if success, discard failures, else return failures (not a dataclass
      // ctor or cross module call)
      llvm_unreachable("cross-module func calls not implemented");
    }
    return this->call(builder);
  }

private:
  const bool await_;
  const bool async_;
  const mpc_ast_t* const ast_;

  static llvm::Expected<small_vector<llvm::Value*>>
  getArgs(const mpc_ast_t* const ast, llvm::IRBuilder<>& builder) {
    small_vector<llvm::Value*> args;
    for (const auto& expr : getExprList(ast)) {
      auto val = expr->codegen(builder);
      if (!val) {
        return val.takeError();
      }
      auto value = *val;
      // @todo FIXME: Maybe value is a addrof value (use tag to identify it??)??
      // @todo: Delegate to Loader, based on use context?
      if (llvm::isa<llvm::GetElementPtrInst>(value)) {
        value = builder.CreateLoad(value);
      }
      args.push_back(value);
    }
    return args;
  }

  static llvm::Error checkFuncCall(const llvm::Value* const value,
                                   llvm::ArrayRef<llvm::Value*> args,
                                   const mpc_state_t state) {
    auto type = value->getType();
    if (type->isPointerTy() && type->getPointerElementType()->isFunctionTy()) {
      type = type->getPointerElementType();
    } else {
      return error("expected `{}` to be callable at line {}",
                   value->getName().str(), state.row + 1);
    }
    const auto funcType = llvm::cast<llvm::FunctionType>(type);
    if (funcType->getNumParams() != args.size()) {
      return error("invalid number of arguments given for function `{}` "
                   "at line {} (expected {}, got {})",
                   value->getName().str(), state.row + 1,
                   funcType->getNumParams(), args.size());
    }

    for (size_t i = 0; i < args.size(); ++i) {
      if (i == 1 && args[i]->getName() == "::expansion") {
        if (args.size() != 2) {
          return error("expected only 2 arguments for partial function "
                       "evaluation at line {} (got {})",
                       state.row + 1, args.size());
        }
        break;
      } else if (args[i]->getName() == "::expansion") {
        return error("cannot use an expansion as argument {} "
                     "in call to function `{}` at line {}",
                     i, value->getName().str(), state.row + 1);
      }
      if (args[i]->getType() != funcType->getParamType(i)) {
        return error("invalid type given for argument {} of call to "
                     "function `{}` at line {}",
                     i + 1, value->getName().str(), state.row + 1);
      }
    }
    return llvm::Error::success();
  }

  llvm::Expected<llvm::Value*> call(llvm::IRBuilder<>& builder) const {
    if (await_) { // @todo enclosing function becomes a coroutine
      llvm_unreachable("awaitable function calls not implemented");
    } else if (async_) { // @todo launch call in a new thread?
      llvm_unreachable("async function calls not implemented");
    }

    small_vector<llvm::Value*> funcs;
    int idx = static_cast<int>(await_ || async_);
    for (; idx < ast_->children_num; ++idx) {
      const auto ref = ast_->children[idx];
      const std::string_view view{ref->contents};
      if (view == "->") {
        continue;
      }
      if (view == "(") {
        ++idx;
        break;
      }
      if (auto func = getFactor(ref)->codegen(builder)) {
        auto fun = *func;
        // @todo: Delegate to Loader, based on use context?
        if (llvm::isa<llvm::AllocaInst>(fun)) {
          fun = builder.CreateLoad(fun);
        }
        funcs.push_back(fun);
      } else {
        return func.takeError();
      }
    }

    const auto partialApply =
        [&builder](llvm::Value* const fun,
                   llvm::Value* const firstArg) -> llvm::Function* {
      const auto func = llvm::cast<llvm::Function>(fun);
      const auto newFuncType = llvm::FunctionType::get(
          func->getReturnType(), func->getFunctionType()->params().drop_front(),
          func->isVarArg());
      return bindFirstFuncArgument(builder, func, firstArg, newFuncType);
    };

    llvm::Value* value;
    for (auto i = 0; idx < ast_->children_num; idx += 2, ++i) {
      const auto ref = ast_->children[idx];
      small_vector<llvm::Value*> arguments;
      if (getOutermostAstTag(ref) == "exprlist") {
        auto args = getArgs(ref, builder);
        if (!args) {
          return args.takeError();
        }
        arguments = std::move(*args);
        ++idx;
      }
      const auto state = ref->state;
      if (i == 0) {
        for (size_t j = 0; j < funcs.size(); ++j) {
          const auto func = funcs[j];
          if (j == 0) {
            if (auto err = checkFuncCall(func, arguments, state)) {
              return err;
            }
            if (arguments.size() == 2 &&
                arguments[1]->getName() == "::expansion") {
              value = partialApply(func, arguments[0]);
            } else {
              value = builder.CreateCall(func, arguments);
            }
          } else {
            if (auto err = checkFuncCall(func, value, state)) {
              return err;
            }
            value = builder.CreateCall(func, value);
          }
        }
      } else {
        if (auto err = checkFuncCall(value, arguments, state)) {
          return err;
        }
        if (arguments.size() == 2 && arguments[1]->getName() == "::expansion") {
          value = partialApply(value, arguments[0]);
        } else {
          value = builder.CreateCall(value, arguments);
        }
      }
    }
    return value;
  }
};

class FuncCallStmt final : public Stmt {
public:
  explicit FuncCallStmt(const mpc_ast_t* const ast) noexcept
      : Stmt(kFuncCall), state_{ast->state}, impl_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto ret = impl_.codegen(builder);
    if (!ret) {
      return ret.takeError();
    }
    if ((*ret)->getType() != BasicTypes["void"]) {
      warning("function return value discarded at line {}", state_.row + 1);
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kFuncCall;
  }

private:
  const mpc_state_t state_;
  const FuncCall impl_;
};

} // end namespace whack::ast

#endif // WHACK_FUNCCALL_HPP
