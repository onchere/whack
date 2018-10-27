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
#ifndef WHACK_STRUCTOP_HPP
#define WHACK_STRUCTOP_HPP

#pragma once

#include "args.hpp"
#include "type.hpp"
#include "typelist.hpp"
#include <variant>

namespace whack::ast {

class StructOp final : public AST {
  using func_name_t = std::variant<std::string, Type>;
  using args_types_t = std::variant<Args, TypeList>;

public:
  explicit StructOp(const mpc_ast_t* const ast) : state_{ast->state} {
    auto idx = 2;
    if (std::string_view(ast->children[idx]->contents) == "mut") {
      mutatesMembers_ = true;
      ++idx;
    }

    structName_ = ast->children[idx++]->contents;
    ++idx;
    auto ref = ast->children[++idx];
    if (getOutermostAstTag(ref) == "type") {
      funcName_ = std::make_unique<func_name_t>(Type{ref});
    } else {
      funcName_ = std::make_unique<func_name_t>(std::string{ref->contents});
    }

    ++idx;
    ref = ast->children[++idx];
    auto tag = getOutermostAstTag(ref);
    if (tag == "args") {
      argsOrTypeList_ = std::make_unique<args_types_t>(Args{ref});
      ++idx;
    } else if (tag == "typelist") {
      argsOrTypeList_ = std::make_unique<args_types_t>(TypeList{ref});
      ++idx;
    }

    if (getOutermostAstTag(ast->children[++idx]) == "type") {
      returnType_ = std::make_unique<Type>(ast->children[idx]);
      ++idx;
    }

    if (getOutermostAstTag(ast->children[idx]) == "tags") {
      tags_ = std::make_unique<Tags>(ast->children[idx++]);
    }

    if (getOutermostAstTag(ast->children[idx]) == "body") {
      body_ = std::make_unique<Body>(ast->children[idx]);
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    const auto structure = module->getTypeByName(structName_);
    if (!structure) {
      return error("cannot find struct `{}` for function "
                   "at line {}",
                   structName_, state_.row + 1);
    }

    // @todo Proper mangling for name
    std::string name;
    if (funcName_->index() == 0) {
      name = format("operator{}", std::get<std::string>(*funcName_));
    } else {
      const auto operatorName = std::get<Type>(*funcName_).str();
      if (operatorName == "auto") {
        return error("function of struct `{}` cannot "
                     "define an operator for deduced type auto "
                     "at line {}",
                     structName_, state_.row + 1);
      }
      name = format("operator {}", operatorName);
    }

    small_vector<llvm::Type*> paramTypes;
    bool variadic = false;
    if (argsOrTypeList_) {
      if (argsOrTypeList_->index() == 0) { // <args>
        const auto args = std::get<Args>(*argsOrTypeList_);
        paramTypes = args.types(module);
        paramTypes.insert(paramTypes.begin(), structure->getPointerTo(0));
        variadic = args.variadic();
      } else { // <typelist>
        std::tie(paramTypes, variadic) =
            std::get<TypeList>(*argsOrTypeList_).codegen(module);
        paramTypes.insert(paramTypes.begin(), structure->getPointerTo(0));
      }
    } else {
      paramTypes = {structure->getPointerTo(0)};
    }

    const auto type = llvm::FunctionType::get(
        Type::getReturnType(module, returnType_), paramTypes, variadic);
    const auto funcName = format("struct::{}::{}", structName_, name);
    auto func = llvm::Function::Create(type, llvm::Function::ExternalLinkage,
                                       funcName, module);
    func->arg_begin()[0].setName("this");
    func->addParamAttr(0, llvm::Attribute::Nest);
    if (!mutatesMembers_) {
      func->addParamAttr(0, llvm::Attribute::ReadOnly);
    }

    if (argsOrTypeList_) {
      if (argsOrTypeList_->index() == 0) { // args
        const auto names = std::get<Args>(*argsOrTypeList_).names();
        for (size_t i = 0; i < names.size(); ++i) {
          func->arg_begin()[i + 1].setName(names[i]);
        }
      }
    }

    auto entry = llvm::BasicBlock::Create(func->getContext(), "entry", func);
    llvm::IRBuilder<> builder{entry};

    if (!body_) {
      // @todo Default the operator; if applicable??
      llvm_unreachable("defauled struct operators not implemented");
    }

    if (auto err = body_->codegen(builder)) {
      return err;
    }

    if (func->back().empty() ||
        !llvm::isa<llvm::ReturnInst>(func->back().back())) {
      if (func->getReturnType() != BasicTypes["void"]) {
        return error("expected function `{}` for struct `{}` "
                     " to have a return value at line {}",
                     name, structName_, state_.row + 1);
      } else {
        builder.CreateRetVoid();
      }
    }

    if (body_) {
      if (auto err = body_->runScopeExit(builder)) {
        return err;
      }

      auto deduced = deduceFuncReturnType(func, state_);
      if (!deduced) { // return better error msg
        return deduced.takeError();
      }

      // we replace func's type if we used return type deduction
      if (func->getReturnType() == BasicTypes["auto"]) {
        (void)changeFuncReturnType(func, *deduced);
        return llvm::Error::success();
      }

      if (*deduced != func->getReturnType()) {
        return error("function `{}` for struct `{}` returns an invalid type "
                     "at line {}",
                     name, structName_, state_.row + 1);
      }
    }
    return llvm::Error::success();
  }

private:
  const mpc_state_t state_;
  bool mutatesMembers_{false};
  std::string structName_;
  std::unique_ptr<func_name_t> funcName_;
  std::unique_ptr<args_types_t> argsOrTypeList_;
  std::unique_ptr<Type> returnType_;
  std::unique_ptr<Tags> tags_;
  std::unique_ptr<Body> body_;
};

} // end namespace whack::ast

#endif // WHACK_STRUCTOP_HPP
