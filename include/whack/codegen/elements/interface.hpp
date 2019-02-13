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
#ifndef WHACK_INTERFACE_HPP
#define WHACK_INTERFACE_HPP

#pragma once

#include "../expressions/factors/identifier.hpp"
#include "../expressions/factors/structmember.hpp"
#include "../types/type.hpp"

namespace whack::codegen::elements {

class Interface final : public AST {
public:
  explicit Interface(const mpc_ast_t* const ast)
      : state_{ast->state}, name_{ast->children[0]->contents} {
    const auto ref = ast->children[1];
    auto idx = 1;
    if (std::string_view(ref->children[idx]->contents) == ":") {
      for (idx = 2; idx < ref->children_num; ++idx) {
        const auto inherit = ref->children[idx];
        const std::string_view view{inherit->contents};
        if (view == ",") {
          continue;
        }
        if (view == "{") {
          break;
        }
        inherits_.emplace_back(expressions::factors::getIdentifier(inherit));
      }
    }
    for (++idx; idx < ref->children_num - 1; idx += 3) {
      functions_.emplace_back(std::pair{types::Type{ref->children[idx]},
                                        ref->children[idx + 1]->contents});
    }
  }

  llvm::Error codegen(llvm::Module* const module) const {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(module, name_)) {
      return err;
    }
    small_vector<llvm::Type*> funcs;
    small_vector<llvm::StringRef> funcNames;
    small_vector<std::pair<llvm::MDNode*, uint64_t>> metadata;
    auto& ctx = module->getContext();
    llvm::MDBuilder MDBuilder{ctx};
    for (const auto& inherit : inherits_) {
      const auto& name = inherit.index() == 1
                             ? std::get<ScopeRes>(inherit).name()
                             : std::get<Ident>(inherit).name();
      auto funcsInfo = getFuncsInfo(module, name);
      if (!funcsInfo) {
        return funcsInfo.takeError();
      }
      std::tie(funcs, funcNames) = std::move(*funcsInfo);
      for (size_t i = 0; i < funcs.size(); ++i) {
        const auto domain = reinterpret_cast<llvm::MDNode*>(
            MDBuilder.createConstant(llvm::Constant::getNullValue(funcs[i])));
        const auto nameMD =
            MDBuilder.createAnonymousAliasScope(domain, funcNames[i]);
        metadata.emplace_back(std::pair{nameMD, i});
      }
    }

    llvm::IRBuilder<> builder{ctx};
    for (const auto& [type, name] : functions_) {
      auto tp = type.codegen(builder);
      if (!tp) {
        return tp.takeError();
      }
      const auto fnType = (*tp)->getPointerTo(0);
      const auto domain = reinterpret_cast<llvm::MDNode*>(
          MDBuilder.createConstant(llvm::Constant::getNullValue(fnType)));
      if (std::find(funcNames.begin(), funcNames.end(), name) !=
          funcNames.end()) {
        return error("interface `{}` already declares function `{}` "
                     "at line {}",
                     name_, name.data(), state_.row + 1);
      }
      funcNames.push_back(name);
      const auto nameMD = MDBuilder.createAnonymousAliasScope(domain, name);
      metadata.emplace_back(std::pair{nameMD, funcs.size()});
      funcs.push_back(fnType);
    }

    const auto impl =
        llvm::StructType::create(ctx, funcs, "interface::" + name_);
    const auto interfaceMD =
        MDBuilder.createTBAAStructTypeNode(name_, metadata);
    module->getOrInsertNamedMetadata("interfaces")->addOperand(interfaceMD);
    addStructTypeMetadata(module, "structures", impl->getName(), funcNames);
    return llvm::Error::success();
  }

  inline const auto& name() const { return name_; }
  inline const auto& functions() const { return functions_; }

  using funcs_info_t =
      std::pair<small_vector<llvm::Type*>, small_vector<llvm::StringRef>>;

  inline static llvm::Expected<funcs_info_t>
  getFuncsInfo(const llvm::Module* const module, llvm::Type* const interface) {
    return getFuncsInfo(module, getName(interface));
  }

  static llvm::Expected<funcs_info_t>
  getFuncsInfo(const llvm::Module* const module,
               llvm::StringRef interfaceName) {
    small_vector<llvm::Type*> funcs;
    small_vector<llvm::StringRef> funcNames;
    if (auto MD = getMetadataOperand(*module, "interfaces", interfaceName)) {
      for (unsigned i = 1; i < MD.value()->getNumOperands(); i += 2) {
        const auto funcMD =
            reinterpret_cast<llvm::MDNode*>(MD.value()->getOperand(i).get());
        const auto func = funcMD->getOperand(1).get();
        funcs.push_back(
            llvm::mdconst::extract<llvm::Constant>(func)->getType());
        const auto funcName =
            reinterpret_cast<llvm::MDString*>(funcMD->getOperand(2).get())
                ->getString();
        funcNames.push_back(funcName);
      }
    } else {
      return error("interface `{}` does not exist", interfaceName.data());
    }
    return std::pair{std::move(funcs), std::move(funcNames)};
  }

  static llvm::Expected<small_vector<llvm::Function*>>
  implements(llvm::IRBuilder<>& builder, llvm::Type* const interface,
             llvm::Value* const value) {
    const auto [type, isStruct] = types::Type::isStructKind(value->getType());
    if (!isStruct) {
      return error("expected value `{}` to be a struct kind",
                   value->getName().data());
    }
    const auto structName = type->getStructName().data();
    const auto module = builder.GetInsertBlock()->getModule();
    auto funcsInfo = getFuncsInfo(module, interface);
    if (!funcsInfo) {
      return funcsInfo.takeError();
    }
    const auto& [funcs, funcNames] = *funcsInfo;
    small_vector<llvm::Function*> funcsImpl;
    for (size_t i = 0; i < funcs.size(); ++i) {
      const auto funcName = funcNames[i].data();
      if (auto structFunc = module->getFunction(
              format("struct::{}::{}", structName, funcName))) {
        using namespace expressions::factors;
        const auto func = StructMember::bindThis(builder, structFunc, value);
        if (func->getType() != funcs[i]) {
          return error("struct `{}` does not implement interface `{}` "
                       "(type mismatch for function `{}`)",
                       structName, getName(interface).data(), funcName);
        }
        funcsImpl.push_back(func);
      } else {
        return error("struct `{}` does not implement interface `{}` "
                     "(no implementation found for function `{}`)",
                     structName, getName(interface).data(), funcName);
      }
    }
    return std::move(funcsImpl);
  }

  /// @brief Casts a struct pointer into an object implementing an interface
  /// checking for a valid implementation
  static llvm::Expected<llvm::Value*> cast(llvm::IRBuilder<>& builder,
                                           llvm::Type* const interface,
                                           llvm::Value* const value) {
    const auto [interfaceType, isStruct] = types::Type::isStructKind(interface);
    assert(isStruct);
    auto impl = implements(builder, interfaceType, value);
    if (!impl) {
      return impl.takeError();
    }
    const auto& funcsImpl = *impl;
    auto interfaceImpl = builder.CreateAlloca(interfaceType, 0, nullptr, "");
    for (size_t i = 0; i < funcsImpl.size(); ++i) {
      const auto ptr =
          builder.CreateStructGEP(interfaceType, interfaceImpl, i, "");
      builder.CreateStore(funcsImpl[i], ptr);
    }
    return interfaceImpl;
  }

private:
  const mpc_state_t state_;
  const std::string name_;
  std::vector<expressions::factors::identifier_t> inherits_;
  std::vector<std::pair<types::Type, llvm::StringRef>> functions_;

  inline static llvm::StringRef getName(llvm::Type* const interface) {
    constexpr static auto nameOffset = std::strlen("interface::");
    return interface->getStructName().substr(nameOffset);
  }
};

} // end namespace whack::codegen::elements

#endif // WHACK_INTERFACE_HPP
