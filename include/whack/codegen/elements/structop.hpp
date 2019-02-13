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
#ifndef WHACK_STRUCTOP_HPP
#define WHACK_STRUCTOP_HPP

#pragma once

#include "../types/type.hpp"
#include <variant>

namespace whack::codegen {

static structopname_t getStructOpName(const mpc_ast_t* const ast) {
  const auto ref = ast->children[1];
  if (getOutermostAstTag(ref) == "type") {
    return types::Type{ref};
  }
  return llvm::StringRef{ref->contents};
}

static llvm::Expected<std::string>
getStructOpNameString(llvm::IRBuilder<>& builder,
                      const structopname_t& funcName) {
  if (funcName.index() == 0) {
    return format("operator {}", std::get<llvm::StringRef>(funcName).data());
  }
  const auto& t = std::get<types::Type>(funcName);
  auto tp = t.codegen(builder);
  if (!tp) {
    return tp.takeError();
  }
  const auto type = *tp;
  if (type == BasicTypes["auto"]) {
    return error("struct function cannot define an "
                 "operator for deduced type auto");
  }
  return format("operator {}", getTypeName(type));
}

namespace elements {

class StructOp final : public AST {
public:
  using args_types_t = std::variant<Args, types::TypeList>;

  explicit StructOp(const mpc_ast_t* const ast) : state_{ast->state} {
    auto idx = 2;
    if (std::string_view(ast->children[idx]->contents) == "mut") {
      mutatesMembers_ = true;
      ++idx;
    }

    structName_ = ast->children[idx++]->contents;
    ++idx;
    funcName_ =
        std::make_unique<structopname_t>(getStructOpName(ast->children[idx]));
    ++idx;
    const auto ref = ast->children[++idx];
    auto tag = getOutermostAstTag(ref);
    if (tag == "args") {
      argsOrTypeList_ = std::make_unique<args_types_t>(Args{ref});
      ++idx;
    } else if (tag == "typelist") {
      argsOrTypeList_ = std::make_unique<args_types_t>(types::TypeList{ref});
      ++idx;
    }

    if (getOutermostAstTag(ast->children[++idx]) == "type") {
      returnType_ = std::make_unique<types::Type>(ast->children[idx]);
      ++idx;
    }

    if (getOutermostAstTag(ast->children[idx]) == "body") {
      body_ = std::make_unique<stmts::Body>(ast->children[idx]);
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    const auto structure = module->getTypeByName(structName_);
    if (!structure) {
      return error("cannot find struct `{}` for function "
                   "at line {}",
                   structName_.data(), state_.row + 1);
    }

    llvm::IRBuilder<> builder{module->getContext()};
    auto name = getStructOpNameString(builder, *funcName_);
    if (!name) {
      return name.takeError();
    }

    const auto funcName = format("struct::{}::{}", structName_.data(), *name);
    if (module->getFunction(funcName)) {
      return error("function `{}` already exists for struct `{}` "
                   "at line {}",
                   *name, structName_.data(), state_.row + 1);
    }

    small_vector<llvm::Type*> paramTypes;
    bool variadic = false;
    if (argsOrTypeList_) {
      if (argsOrTypeList_->index() == 0) { // <args>
        const auto args = std::get<Args>(*argsOrTypeList_);
        auto types = args.types(builder);
        if (!types) {
          return types.takeError();
        }
        paramTypes = std::move(*types);
        paramTypes.insert(paramTypes.begin(), structure->getPointerTo(0));
        variadic = args.variadic();
      } else { // <typelist>
        auto typeList =
            std::get<types::TypeList>(*argsOrTypeList_).codegen(builder);
        if (!typeList) {
          return typeList.takeError();
        }
        std::tie(paramTypes, variadic) = std::move(*typeList);
        paramTypes.insert(paramTypes.begin(), structure->getPointerTo(0));
      }
    } else {
      paramTypes = {structure->getPointerTo(0)};
    }

    auto returnType = types::Type::getReturnType(builder, returnType_);
    if (!returnType) {
      return returnType.takeError();
    }
    const auto type =
        llvm::FunctionType::get(*returnType, paramTypes, variadic);
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

    if (!body_) {
      // @todo Default the operator; if applicable??
      return error("defaulted struct operators not implemented "
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
  llvm::StringRef structName_;
  std::unique_ptr<structopname_t> funcName_;
  std::unique_ptr<args_types_t> argsOrTypeList_;
  std::unique_ptr<types::Type> returnType_;
  std::unique_ptr<stmts::Body> body_;
};

} // end namespace elements
} // end namespace whack::codegen

#endif // WHACK_STRUCTOP_HPP
