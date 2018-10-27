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
#ifndef WHACK_CLOSURE_HPP
#define WHACK_CLOSURE_HPP

#pragma once

#include "args.hpp"
#include "body.hpp"
#include "metadata.hpp"
#include "structure.hpp"
#include "tags.hpp"
#include "typelist.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::ast {

class Closure final : public Factor {
public:
  explicit Closure(const mpc_ast_t* const ast)
      : Factor(kClosure), state_{ast->state} {
    auto idx = 3;
    if (std::string_view(ast->children[1]->contents) == "[") {
      for (auto i = 2; i < ast->children_num; ++i) {
        // @todo
      }
    }
    if (getOutermostAstTag(ast->children[2]) == "args") {
      args_ = std::make_unique<Args>(ast->children[2]);
      ++idx;
    }
    if (ast->children_num > 4 &&
        getOutermostAstTag(ast->children[idx]) == "typelist") {
      returns_ = std::make_unique<TypeList>(ast->children[idx]);
      ++idx;
    }
    if (ast->children_num > 5 &&
        getOutermostAstTag(ast->children[idx]) == "tags") {
      tags_ = std::make_unique<Tags>(ast->children[idx]);
      ++idx;
    }
    body_ = std::make_unique<Body>(ast->children[idx]);
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto enclosingFn = builder.GetInsertBlock()->getParent();
    const auto module = enclosingFn->getParent();
    auto type = Type::getReturnType(
        module->getContext(),
        returns_ ? std::optional{returns_->codegen(module)} : std::nullopt,
        state_);
    if (!type) {
      return type.takeError();
    }
    const auto returnType = *type;
    small_vector<llvm::Type*> scopedTypes;
    small_vector<llvm::Value*> scopedValues;
    small_vector<llvm::StringRef> scopedNames;

    // we inherit any captured variables if enclosing function is a closure
    if (enclosingFn->getName().startswith("::closure")) {
      const auto enclosingEnv =
          llvm::cast<llvm::Value>(&enclosingFn->arg_begin()[0]);
      const auto enclosingEnvType =
          enclosingEnv->getType()->getPointerElementType();
      scopedNames = getMetadataParts(*module, "structures",
                                     enclosingEnvType->getStructName());
      for (size_t i = 0; i < scopedNames.size(); ++i) {
        const auto ptr =
            builder.CreateStructGEP(enclosingEnvType, enclosingEnv, i, "");
        const auto val = builder.CreateLoad(ptr);
        scopedValues.push_back(val);
        scopedTypes.push_back(val->getType());
      }
    }

    for (const auto& symbol : *enclosingFn->getValueSymbolTable()) {
      const auto val = symbol.getValue();
      if (!val->getType()->isSized() || val->getName().find('.')) {
        continue;
      }
      scopedValues.push_back(val);
      scopedTypes.push_back(val->getType());
      scopedNames.push_back(val->getName());
    }

    const auto env = llvm::StructType::create(module->getContext());
    small_vector<llvm::Type*> argTypes;
    if (args_) {
      argTypes = args_->types(module);
      argTypes.insert(argTypes.begin(), env->getPointerTo(0));
    } else {
      argTypes = {env->getPointerTo(0)};
    }

    auto func = llvm::Function::Create(
        llvm::FunctionType::get(returnType, argTypes,
                                args_ ? args_->variadic() : false),
        llvm::Function::ExternalLinkage, "::closure", module);
    func->arg_begin()[0].setName(".env");
    func->addParamAttr(0, llvm::Attribute::Nest);
    if (args_) {
      const auto names = args_->names();
      for (size_t i = 0; i < names.size(); ++i) {
        if (std::find(scopedNames.begin(), scopedNames.end(), names[i]) !=
            scopedNames.end()) {
          return error("parameter name `{}` for closure already exists "
                       "as a scoped variable name at line {}",
                       names[i].str(), state_.row + 1);
        }
        func->arg_begin()[i + 1].setName(names[i]);
      }
      for (size_t i = 0; i < func->arg_size() - 1; ++i) {
        if (!args_->arg(i).mut) {
          func->addParamAttr(i + 1, llvm::Attribute::ReadOnly);
        }
      }
    }

    env->setName(func->getName());
    env->setBody(scopedTypes);
    Structure::addMetadata(module, env->getName(), scopedNames);

    const auto entry =
        llvm::BasicBlock::Create(func->getContext(), "entry", func);
    llvm::IRBuilder<> tmpBuilder{entry};
    if (auto err = body_->codegen(tmpBuilder)) {
      return err;
    }
    if (auto err = body_->runScopeExit(tmpBuilder)) {
      return err;
    }

    auto deduced = deduceFuncReturnType(func, state_);
    if (!deduced) {
      return deduced.takeError();
    }

    // we replace func's type if we used return type deduction
    if (func->getReturnType() == BasicTypes["auto"]) {
      func = changeFuncReturnType(func, *deduced);
    } else { // we check return type for errors
      if (*deduced != func->getReturnType()) {
        return error("closure returns an invalid type "
                     "at line {}",
                     state_.row + 1);
      }
    }

    // @todo: capture appropriate variables if defined explicitly
    const auto scopeVars = builder.CreateAlloca(env, 0, nullptr, "");
    for (size_t i = 0; i < scopedValues.size(); ++i) {
      const auto ptr = builder.CreateStructGEP(env, scopeVars, i, "");
      // we create copies as a default: @todo
      builder.CreateStore(scopedValues[i], ptr);
    }

    const auto closureType = llvm::FunctionType::get(
        *deduced, func->getFunctionType()->params().drop_front(),
        func->isVarArg());
    return bindFirstFuncArgument(builder, func, scopeVars, closureType);
  }

private:
  const mpc_state_t state_;
  std::unique_ptr<Args> args_;
  llvm::StringMap<std::unique_ptr<Factor>> captures_;
  std::unique_ptr<TypeList> returns_;
  std::unique_ptr<Tags> tags_;
  std::unique_ptr<Body> body_;
};

} // end namespace whack::ast

#endif // WHACK_CLOSURE_HPP
