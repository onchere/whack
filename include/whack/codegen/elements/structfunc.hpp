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
#ifndef WHACK_STRUCTFUNC_HPP
#define WHACK_STRUCTFUNC_HPP

#pragma once

#include "../expressions/factors/structmember.hpp"
#include <llvm/IR/Attributes.h>

namespace whack::codegen::elements {

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
      returns_ = std::make_unique<types::TypeList>(ast->children[idx++]);
    }

    if (getOutermostAstTag(ast->children[idx]) == "body") {
      body_ = std::make_unique<stmts::Body>(ast->children[idx]);
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

    using namespace expressions::factors;
    if (StructMember::getIndex(*module, structName_, funcName_)) {
      return error("member `{}` already exists for struct `{}` at line {}",
                   funcName_, structName_, state_.row + 1);
    }

    const auto name = format("struct::{}::{}", structName_, funcName_);
    if (module->getFunction(name)) {
      return error("function `{}` already exists for struct `{}` "
                   "at line {}",
                   funcName_, structName_, state_.row + 1);
    }

    auto& ctx = module->getContext();
    llvm::IRBuilder builder{ctx};
    std::optional<types::typelist_t> retTypeList{std::nullopt};
    if (returns_) {
      auto type = returns_->codegen(builder);
      if (!type) {
        return type.takeError();
      }
      retTypeList = std::move(*type);
    }

    auto returnType = types::Type::getReturnType(ctx, retTypeList, state_);
    if (!returnType) {
      return returnType.takeError();
    }
    llvm::FunctionType* type;
    if (args_) {
      auto tps = args_->types(builder);
      if (!tps) {
        return tps.takeError();
      }
      auto& types = *tps;
      types.insert(types.begin(), structure->getPointerTo(0));
      type = llvm::FunctionType::get(*returnType, types, args_->variadic());
    } else {
      type = llvm::FunctionType::get(*returnType, structure->getPointerTo(0),
                                     false);
    }

    auto func = llvm::Function::Create(type, llvm::Function::ExternalLinkage,
                                       name, module);
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

    if (!body_) {
      auto entry = llvm::BasicBlock::Create(ctx, "entry", func);
      llvm::IRBuilder<> builder{entry};
      if (funcName_ == "__ctor") {
        // @todo Default the constructor (zero init the members?)
      } else { // "__dtor"
               // @todo
      }
      return error("defaulted struct ctor/dtor not implemented "
                   "at line {}",
                   state_.row + 1);
    }

    auto built = buildFunction(func, body_.get());
    if (!built) {
      return built.takeError();
    }
    return llvm::Error::success();
  }

private:
  const mpc_state_t state_;
  bool mutatesMembers_{false};
  std::string structName_;
  std::string funcName_;
  std::unique_ptr<Args> args_;
  std::unique_ptr<types::TypeList> returns_;
  std::unique_ptr<stmts::Body> body_;
};

} // end namespace whack::codegen::elements

#endif // WHACK_STRUCTFUNC_HPP
