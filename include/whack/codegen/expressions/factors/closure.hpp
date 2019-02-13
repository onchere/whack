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
#ifndef WHACK_CLOSURE_HPP
#define WHACK_CLOSURE_HPP

#pragma once

#include "../../elements/args.hpp"
#include <llvm/IR/ValueSymbolTable.h>

namespace whack::codegen::expressions::factors {

class Closure final : public Factor {
public:
  enum DefaultCaptureMode { None, AllByValue, AllByReference };

  explicit Closure(const mpc_ast_t* const ast)
      : Factor(kClosure), state_{ast->state} {
    auto idx = 2;
    if (std::string_view(ast->children[1]->contents) == "[") {
      for (idx = 2; idx < ast->children_num; idx += 2) {
        const auto ref = ast->children[idx];
        if (getOutermostAstTag(ref) != "capture") {
          ++idx;
          break;
        }
        if (ref->children_num) {
          const auto name = ref->children[0]->contents;
          // @todo
          const bool isRef =
              std::string_view(ref->children[1]->contents) == "&";
          explicitCaptures_[name] =
              getExpressionValue(ref->children[isRef ? 0 : 2]);
        } else {
          const std::string_view view{ref->contents};
          if (view == "=") {
            if (defaultCaptureMode_ != None) {
              warning("default capture mode for closure re-declared "
                      "at line {}",
                      ref->state.row + 1);
            }
            defaultCaptureMode_ = AllByValue;
          } else if (view == "&") {
            if (defaultCaptureMode_ != None) {
              warning("default capture mode for closure re-declared "
                      "at line {}",
                      ref->state.row + 1);
            }
            defaultCaptureMode_ = AllByReference;
          } else {
            explicitCaptures_[ref->contents] = getExpressionValue(ref);
          }
        }
      }
    }
    if (getOutermostAstTag(ast->children[idx]) == "args") {
      args_ = std::make_unique<elements::Args>(ast->children[idx]);
      ++idx;
    }
    if (getOutermostAstTag(ast->children[++idx]) == "typelist") {
      returns_ = std::make_unique<types::TypeList>(ast->children[idx]);
      ++idx;
    }
    body_ = std::make_unique<stmts::Body>(ast->children[idx]);
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto enclosingFn = builder.GetInsertBlock()->getParent();
    const auto module = enclosingFn->getParent();
    std::optional<types::typelist_t> returns{std::nullopt};
    if (returns_) {
      auto typeList = returns_->codegen(builder);
      if (!typeList) {
        return typeList.takeError();
      }
      returns = std::move(*typeList);
    }
    auto returnType =
        types::Type::getReturnType(module->getContext(), returns, state_);
    if (!returnType) {
      return returnType.takeError();
    }

    small_vector<llvm::Type*> scopedTypes;
    small_vector<llvm::Value*> scopedValues;
    small_vector<llvm::StringRef> scopedNames;

    if (defaultCaptureMode_ == AllByReference) {
      return error("capturing all closure variable(s) explicitly"
                   " by reference is not implemented at line {}",
                   state_.row + 1);
    } else if (defaultCaptureMode_ == AllByValue) {
      // we inherit any captured variables if enclosing function is a closure
      if (enclosingFn->getName().startswith("::closure")) {
        if (!enclosingFn->arg_empty()) {
          const auto enclosingEnv =
              llvm::cast<llvm::Value>(&enclosingFn->arg_begin()[0]);
          // we ensure first parameter is an environment structure
          if (enclosingEnv->getName() == ".env") {
            const auto enclosingEnvType =
                enclosingEnv->getType()->getPointerElementType();
            scopedNames = getMetadataParts(*module, "structures",
                                           enclosingEnvType->getStructName());
            for (size_t i = 0; i < scopedNames.size(); ++i) {
              const auto ptr = builder.CreateStructGEP(enclosingEnvType,
                                                       enclosingEnv, i, "");
              const auto val = builder.CreateLoad(ptr);
              scopedValues.push_back(val);
              scopedTypes.push_back(val->getType());
            }
          }
        }
      }
      for (const auto& symbol : *enclosingFn->getValueSymbolTable()) {
        const auto val = symbol.getValue();
        if (!val->getType()->isSized() ||
            val->getName().find('.') != llvm::StringRef::npos) {
          continue;
        }
        scopedValues.push_back(val);
        scopedTypes.push_back(val->getType());
        scopedNames.push_back(val->getName());
      }
    }

    for (const auto& capture : explicitCaptures_) {
      const auto name = capture.getKey();
      if (std::find(scopedNames.begin(), scopedNames.end(), name) !=
          scopedNames.end()) {
        return error("variable name `{}` already in use "
                     "for closure capture list at line {}",
                     name.str(), state_.row + 1);
      }
      scopedNames.push_back(name);
      auto val = capture.getValue()->codegen(builder);
      if (!val) {
        return val.takeError();
      }
      const auto value = *val;
      scopedValues.push_back(value);
      scopedTypes.push_back(value->getType());
    }

    small_vector<llvm::Type*> argTypes;
    if (args_) {
      auto types = args_->types(builder);
      if (!types) {
        return types.takeError();
      }
      argTypes = std::move(*types);
    }

    const auto hasEnv = !scopedValues.empty();
    if (hasEnv) {
      argTypes.insert(
          argTypes.begin(),
          llvm::StructType::create(module->getContext())->getPointerTo(0));
    }

    auto func = llvm::Function::Create(
        llvm::FunctionType::get(*returnType, argTypes,
                                args_ ? args_->variadic() : false),
        llvm::Function::ExternalLinkage, "::closure", module);

    if (hasEnv) {
      func->arg_begin()[0].setName(".env");
      func->addParamAttr(0, llvm::Attribute::Nest);
    }

    if (args_) {
      const auto j = static_cast<int>(hasEnv);
      const auto names = args_->names();
      for (size_t i = 0; i < names.size(); ++i) {
        if (hasEnv && std::find(scopedNames.begin(), scopedNames.end(),
                                names[i]) != scopedNames.end()) {
          return error("parameter name `{}` for closure already exists "
                       "as a scoped variable name at line {}",
                       names[i].str(), state_.row + 1);
        }
        func->arg_begin()[i + j].setName(names[i]);
      }
      for (size_t i = 0; i < func->arg_size() - j; ++i) {
        if (!args_->arg(i).mut) {
          func->addParamAttr(i + j, llvm::Attribute::ReadOnly);
        }
      }
    }

    if (hasEnv) {
      const auto env = llvm::cast<llvm::StructType>(
          argTypes.front()->getPointerElementType());
      env->setName(func->getName());
      env->setBody(scopedTypes);
      addStructTypeMetadata(module, "structures", env->getName(), scopedNames);
    }

    auto built = buildFunction(func, body_.get());
    if (!built) {
      return built.takeError();
    }
    func = *built;
    if (!hasEnv) {
      return func;
    }

    const auto env = argTypes.front()->getPointerElementType();
    const auto scopeVars = builder.CreateAlloca(env, 0, nullptr, "");
    for (size_t i = 0; i < scopedValues.size(); ++i) {
      const auto ptr = builder.CreateStructGEP(env, scopeVars, i, "");
      builder.CreateStore(scopedValues[i], ptr);
    }
    const auto closureType = llvm::FunctionType::get(
        func->getReturnType(), func->getFunctionType()->params().drop_front(),
        func->isVarArg());
    return bindFirstFuncArgument(builder, func, scopeVars, closureType);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kClosure;
  }

private:
  const mpc_state_t state_;
  std::unique_ptr<elements::Args> args_;
  llvm::StringMap<expr_t> explicitCaptures_;
  DefaultCaptureMode defaultCaptureMode_{None};
  std::unique_ptr<types::TypeList> returns_;
  std::unique_ptr<stmts::Body> body_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_CLOSURE_HPP
