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
#ifndef WHACK_STRUCTFUNC_HPP
#define WHACK_STRUCTFUNC_HPP

#pragma once

#include "args.hpp"
#include "body.hpp"
#include "tags.hpp"
#include "typelist.hpp"
#include <llvm/IR/Attributes.h>

namespace whack::ast {

class StructFunc final : public AST {
public:
  explicit StructFunc(const mpc_ast_t* const ast) : state_{ast->state} {
    auto idx = 2;
    if (std::string_view(ast->children[idx]->contents) == "mut") {
      mutatesMembers_ = true;
      ++idx;
    }

    structName_ = ast->children[idx++]->contents;
    funcName_ = ast->children[++idx]->contents;
    ++idx;

    if (getOutermostAstTag(ast->children[++idx]) == "args") {
      args_ = std::make_unique<Args>(ast->children[idx++]);
    }

    if (getOutermostAstTag(ast->children[++idx]) == "typelist") {
      returns_ = std::make_unique<TypeList>(ast->children[idx++]);
    }

    if (getOutermostAstTag(ast->children[idx]) == "tags") {
      tags_ = std::make_unique<Tags>(ast->children[idx++]);
    }

    if (getOutermostAstTag(ast->children[idx]) == "body") {
      body_ = std::make_unique<Body>(ast->children[idx]);
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    // @todo Allow operator overloads(for comparators) to be defaulted
    if (!body_ && !(funcName_ == "__ctor" || funcName_ == "__dtor")) {
      warning("only a struct ctor/dtor can be "
              "defaulted at line {}",
              state_.row + 1);
    }
    const auto structure = module->getTypeByName(structName_);
    if (!structure) {
      return error("cannot find struct `{}` for function `{}` "
                   "at line {}",
                   structName_, funcName_, state_.row + 1);
    }

    const auto name = format("struct::{}::{}", structName_, funcName_);
    if (module->getFunction(name)) {
      return error("function `{}` already exists for struct `{}` "
                   "at line {}",
                   funcName_, structName_, state_.row + 1);
    }

    auto returnType = Type::getReturnType(
        module->getContext(),
        returns_ ? std::optional{returns_->codegen(module)} : std::nullopt,
        state_);

    if (!returnType) {
      return returnType.takeError();
    }

    llvm::FunctionType* type;
    if (args_) {
      auto types = args_->types(module);
      types.insert(types.begin(), structure->getPointerTo(0));
      type = llvm::FunctionType::get(*returnType, types, args_->variadic());
    } else {
      type = llvm::FunctionType::get(*returnType, structure->getPointerTo(0),
                                     false);
    }

    const auto func = llvm::Function::Create(
        type, llvm::Function::ExternalLinkage, name, module);
    func->arg_begin()[0].setName("this");
    func->addParamAttr(0, llvm::Attribute::Nest);
    if (!mutatesMembers_) {
      func->addParamAttr(0, llvm::Attribute::ReadOnly);
    }

    if (args_) {
      const auto names = args_->names();
      for (size_t i = 0; i < names.size(); ++i) {
        func->arg_begin()[i + 1].setName(names[i]);
      }
      for (size_t i = 0; i < func->arg_size() - 1; ++i) {
        if (!args_->arg(i).mut) {
          func->addParamAttr(i + 1, llvm::Attribute::ReadOnly);
        }
      }
    }

    auto entry = llvm::BasicBlock::Create(func->getContext(), "entry", func);
    llvm::IRBuilder<> builder{entry};
    if (body_) {
      if (auto err = body_->codegen(builder)) {
        return err;
      }
    } else {
      if (funcName_ == "__ctor") {
        // @todo Default the constructor (zero init the members?)
        // builder.CreateStore(llvm::Constant::getNullValue(), func->arg(0))
      } else { // "__dtor"
               // @todo
      }
      llvm_unreachable("struct defaulted ctor/dtor not implemented");
    }

    if (func->back().empty() ||
        !llvm::isa<llvm::ReturnInst>(func->back().back())) {
      if (func->getReturnType() != BasicTypes["void"]) {
        return error("expected function `{}` for struct `{}` to "
                     " have a return value at line {}",
                     funcName_, structName_, state_.row + 1);
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
                     funcName_, structName_, state_.row + 1);
      }
    }
    return llvm::Error::success();
  }

private:
  const mpc_state_t state_;
  bool mutatesMembers_{false};
  std::string structName_;
  std::string funcName_;
  std::unique_ptr<Args> args_;
  std::unique_ptr<TypeList> returns_;
  std::unique_ptr<Tags> tags_;
  std::unique_ptr<Body> body_;
};

} // end namespace whack::ast

#endif // WHACK_STRUCTFUNC_HPP
