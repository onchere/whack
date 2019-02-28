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
#include "expansion.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::codegen::expressions::factors {

class FuncCall {
public:
  static llvm::Expected<llvm::Value*>
  get(llvm::IRBuilder<>& builder, small_vector<llvm::Value*> funcs,
      small_vector<llvm::Value*> arguments) {
    llvm::Value* value = nullptr;
    for (size_t j = 0; j < funcs.size(); ++j) {
      const auto func = funcs[j];
      if (j == 0) {
        if (auto err = reorderArgs(builder, func, arguments)) {
          return err;
        }
        if (auto err = checkTransformArgs(builder, func, arguments)) {
          return err;
        }
        if (!arguments.empty() && llvm::isa<Expansion>(arguments.back())) {
          arguments.pop_back();
          if (auto apply = partialApply(builder, func, arguments)) {
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
    return value;
  }

private:
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

  static llvm::Expected<llvm::Function*>
  partialApply(llvm::IRBuilder<>& builder, llvm::Value* const fun,
               const llvm::ArrayRef<llvm::Value*> args) {
    auto func = llvm::cast<llvm::Function>(fun);
    const auto numParams = func->getFunctionType()->params().size();
    if (numParams <= args.size()) {
      return error("cannot partially applicate function `{}` "
                   "(number of arguments exceeds {}, got {}) ",
                   func->getName().data(), numParams, args.size());
    }
    for (const auto arg : args) {
      const auto newFuncType = llvm::FunctionType::get(
          func->getReturnType(), func->getFunctionType()->params().drop_front(),
          func->isVarArg());
      func = bindFirstFuncArgument(builder, func, arg, newFuncType);
    }
    return func;
  }

  static llvm::Error reorderArgs(llvm::IRBuilder<>& builder,
                                 llvm::Value* const fun,
                                 small_vector<llvm::Value*>& arguments) {
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
                   "(expected {}, got {})",
                   funcName, func->arg_size(), numElems);
    }
    const auto& module = *builder.GetInsertBlock()->getModule();
    const auto members =
        getMetadataParts(module, "structures", type->getName());
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
        return error("argument `{}` does not exist for function `{}` ",
                     members[i].data(), funcName);
      }
    }
    arguments = std::move(reorderedArgs);
    return llvm::Error::success();
  }
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_FUNCCALL_HPP
