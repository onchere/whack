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
#ifndef WHACK_FUNCCALL_HPP
#define WHACK_FUNCCALL_HPP

#pragma once

#include "../../elements/dataclass.hpp"
#include "../../elements/interface.hpp"
#include <iostream>
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::codegen::expressions::factors {

class FuncCall final : public Factor {
public:
  explicit constexpr FuncCall(const mpc_ast_t* const ast)
      : Factor(kFuncCall),
        // clang-format off
        await_{std::string_view(ast->children[0]->contents) == "await"},
        async_{!await_ &&
               std::string_view(ast->children[0]->contents) == "async"},
        // clang-format on
        ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    if (!await_ && !async_) {
      const auto clazz = ast_->children[0];
      using namespace elements;
      if (DataClass::isa(clazz, builder.GetInsertBlock()->getModule())) {
        if (auto val = DataClass::construct(clazz, builder)) {
          return *val;
        } else {
          return val.takeError();
        }
      }
    }
    return this->call(builder);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kFuncCall;
  }

private:
  const bool await_;
  const bool async_;
  const mpc_ast_t* const ast_;

  static llvm::Error checkTransformArgs(llvm::IRBuilder<>& builder,
                                        llvm::Value* const value,
                                        small_vector<llvm::Value*>& args) {
    const auto valueName = value->getName().data();
    auto type = value->getType();
    if (type->isPointerTy() && type->getPointerElementType()->isFunctionTy()) {
      type = type->getPointerElementType();
    } else {
      return error("expected `{}` to be callable", valueName);
    }

    const auto funcType = llvm::cast<llvm::FunctionType>(type);
    if (funcType->isVarArg()) {
      // @todo
      return llvm::Error::success();
    }

    if (!args.size()) {
      return llvm::Error::success();
    }

    if (!llvm::isa<Expansion>(args.back()) &&
        funcType->getNumParams() != args.size()) {
      return error("invalid number of arguments given for function `{}` "
                   "(expected {}, got {})",
                   valueName, funcType->getNumParams(), args.size());
    }

    for (size_t i = 0; i < args.size(); ++i) {
      if (llvm::isa<Expansion>(args[i])) {
        if (i != args.size() - 1) {
          return error("expected the expansion as the last argument "
                       "in call to function `{}`",
                       i, valueName);
        }
        break;
      }

      // @todo Check if we need an operator overload?
      // @todo Check if we need an implicit cast (+warning)?
      const auto paramType = funcType->getParamType(i);
      const auto [type, isStruct] = types::Type::isStructKind(paramType);
      if (isStruct && type->getStructName().startswith("interface::")) {
        auto impl = elements::Interface::cast(builder, paramType, args[i]);
        if (!impl) {
          return impl.takeError();
        }
        args[i] = *impl;
      } else if (args[i]->getType() != paramType) {
        return error("invalid type given for argument {} of call to "
                     "function `{}`",
                     i + 1, valueName);
      }
    }
    return llvm::Error::success();
  }

  llvm::Expected<llvm::Value*> call(llvm::IRBuilder<>& builder) const {
    if (await_) { // @todo enclosing function becomes a coroutine
      return error("awaitable function calls not implemented at line {}",
                   ast_->state.row + 1);
    } else if (async_) { // @todo launch call in a new thread?
      return error("async function calls not implemented at line {}",
                   ast_->state.row + 1);
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
        auto fun = getLoadedValue(builder, *func);
        if (!fun) {
          return fun.takeError();
        }
        funcs.push_back(*fun);
      } else {
        return func.takeError();
      }
    }

    const auto partialApply = [&](llvm::Value* const fun,
                                  const llvm::ArrayRef<llvm::Value*> args)
        -> llvm::Expected<llvm::Function*> {
      auto func = llvm::cast<llvm::Function>(fun);
      const auto numParams = func->getFunctionType()->params().size();
      if (numParams <= args.size()) {
        return error("cannot partially applicate function `{}` "
                     "(number of arguments exceeds {}, got {}) "
                     "at line {}",
                     func->getName().data(), numParams, args.size(),
                     ast_->state.row + 1);
      }
      for (const auto arg : args) {
        const auto newFuncType = llvm::FunctionType::get(
            func->getReturnType(),
            func->getFunctionType()->params().drop_front(), func->isVarArg());
        func = bindFirstFuncArgument(builder, func, arg, newFuncType);
      }
      return func;
    };

    const auto module = builder.GetInsertBlock()->getParent()->getParent();
    const auto reorderArgs =
        [&](llvm::Value* const fun,
            small_vector<llvm::Value*>& arguments) -> llvm::Error {
      // we only reorder named func args
      if (arguments.size() != 1 || !arguments[0]->getType()->isStructTy() ||
          !arguments[0]->getType()->getStructName().startswith(
              "::memberinitlist")) {
        return llvm::Error::success();
      }
      const auto type = llvm::cast<llvm::StructType>(arguments[0]->getType());
      const auto numElems = type->getNumElements();
      const auto func = llvm::cast<llvm::Function>(fun);
      const auto funcName = func->getName().data();
      if (numElems != func->arg_size()) {
        return error("invalid number of named arguments for function `{}` "
                     "at line {} (expected {}, got {})",
                     funcName, ast_->state.row + 1, func->arg_size(), numElems);
      }
      const auto members =
          getMetadataParts(*module, "structures", type->getName());
      small_vector<llvm::Value*> reorderedArgs{numElems};
      const auto obj =
          llvm::cast<llvm::LoadInst>(arguments[0])->getPointerOperand();
      for (size_t i = 0; i < numElems; ++i) {
        bool found = false;
        for (size_t j = 0; j < func->arg_size(); ++j) {
          if (members[i] == func->arg_begin()[j].getName()) {
            const auto ptr = builder.CreateStructGEP(type, obj, i, "");
            reorderedArgs[j] = builder.CreateLoad(ptr);
            found = true;
            break;
          }
        }
        if (!found) {
          return error("argument `{}` does not exist for function `{}` "
                       "at line {}",
                       members[i].data(), funcName, ast_->state.row + 1);
        }
      }
      arguments = std::move(reorderedArgs);
      return llvm::Error::success();
    };

    llvm::Value* value = nullptr;
    for (auto i = 0; idx < ast_->children_num; idx += 2, ++i) {
      const auto ref = ast_->children[idx];
      small_vector<llvm::Value*> arguments;
      if (getOutermostAstTag(ref) == "exprlist") {
        const auto exprList = getExprList(ref);
        auto args = getExprValues(builder, exprList, true);
        if (!args) {
          return args.takeError();
        }
        arguments = std::move(*args);
        ++idx;
      }

      if (i == 0) {
        for (size_t j = 0; j < funcs.size(); ++j) {
          const auto func = funcs[j];
          if (j == 0) {
            if (auto err = reorderArgs(func, arguments)) {
              return err;
            }
            if (auto err = checkTransformArgs(builder, func, arguments)) {
              return err;
            }
            if (!arguments.empty() && llvm::isa<Expansion>(arguments.back())) {
              arguments.pop_back();
              if (auto apply = partialApply(func, arguments)) {
                value = *apply;
              } else {
                return apply.takeError();
              }
            } else {
              value = builder.CreateCall(func, arguments);
            }
          } else {
            arguments = {value};
            if (auto err = checkTransformArgs(builder, func, arguments)) {
              return err;
            }
            value = builder.CreateCall(func, arguments);
          }
        }
      } else {
        if (auto err = reorderArgs(value, arguments)) {
          return err;
        }
        if (auto err = checkTransformArgs(builder, value, arguments)) {
          return err;
        }
        if (!arguments.empty() && llvm::isa<Expansion>(arguments.back())) {
          arguments.pop_back();
          if (auto apply = partialApply(value, arguments)) {
            value = *apply;
          } else {
            return apply.takeError();
          }
        } else {
          value = builder.CreateCall(value, arguments);
        }
      }
    }
    return value;
  }
};

class FuncCallStmt final : public stmts::Stmt {
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

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_FUNCCALL_HPP
