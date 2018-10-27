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
#ifndef WHACK_DATACLASS_HPP
#define WHACK_DATACLASS_HPP

#pragma once

#include "ast.hpp"
#include "character.hpp"
#include "metadata.hpp"
#include "type.hpp"

namespace whack::ast {

class DataClass final : public AST {
public:
  explicit DataClass(const mpc_ast_t* const ast)
      : state_{ast->state}, class_{ast->children[1]->contents} {
    const auto ref = ast->children[2];
    for (auto i = 2; i < ref->children_num - 4; i += 4) {
      const llvm::StringRef name{ref->children[i]->contents};
      if (getOutermostAstTag(ref->children[i + 2]) == "typelist") {
        TypeList typeList{ref->children[i + 2]};
        variants_.emplace_back(std::pair{name, std::move(typeList)});
        ++i;
      } else {
        variants_.emplace_back(std::pair{name, std::nullopt});
      }
    }
  }

  // registers a data class type
  llvm::Error codegen(llvm::Module* const module) const {
    if (module->getTypeByName(class_) ||
        module->getTypeByName("class::" + class_)) {
      return error("module `{}` already has a type with the given "
                   " name `{}` for data class at line {}",
                   module->getModuleIdentifier(), class_, state_.row + 1);
    }

    auto& ctx = module->getContext();
    llvm::MDBuilder MDBuilder{ctx};
    unsigned int biggestSize = 8;
    const auto dataClass = llvm::StructType::create(ctx, "class::" + class_);

    small_vector<std::pair<llvm::MDNode*, uint64_t>> metadata;
    for (const auto& [name, typeList] : variants_) {
      const auto classMD =
          reinterpret_cast<llvm::MDNode*>(MDBuilder.createString(name));
      metadata.emplace_back(std::make_pair(classMD, metadata.size()));
      // @todo Proper mangling
      const auto className = format("class::{}::{}", class_, name.str());
      if (typeList) {
        auto [types, variadic] = typeList.value().codegen(module);
        if (variadic) {
          return error("cannot use variadic type in typelist for "
                       "constructor `{}` in data class `{}` "
                       "at line {}",
                       name.data(), class_, state_.row + 1);
        }
        types.insert(types.begin(), BasicTypes["char"]); // for tag
        const auto variant = llvm::StructType::create(ctx, types, className);
        const auto size = Type::getBitSize(module, variant);
        if (size > biggestSize) {
          biggestSize = size;
        }
      } else {
        (void)llvm::StructType::create(ctx, BasicTypes["char"] /*tag*/,
                                       className);
      }
    }

    dataClass->setBody({BasicTypes["char"],
                        llvm::ArrayType::get(BasicTypes["char"], biggestSize)});
    auto MD = module->getOrInsertNamedMetadata("classes");
    MD->addOperand(MDBuilder.createTBAAStructTypeNode(class_, metadata));
    return llvm::Error::success();
  }

  // constructs a value by selecting a data class ctor
  static llvm::Expected<llvm::Value*> construct(const mpc_ast_t* const ast,
                                                llvm::IRBuilder<>& builder) {
    const auto ref = ast->children[0];
    const auto className = ref->children[0]->contents;
    const auto ctorName = ref->children[2]->contents;
    // @todo Proper mangling
    const auto dataClass = format("class::{}::{}", className, ctorName);
    const auto module = builder.GetInsertBlock()->getModule();

    if (const auto type = module->getTypeByName(dataClass)) {
      auto alloc = builder.CreateAlloca(type, 0, nullptr, dataClass);
      const auto idx = DataClass::getIndex(module, className, ctorName);
      assert(idx.value() && "invalid constructor index for data class");
      const auto tag = Character::get(idx.value());
      builder.CreateStore(tag, builder.CreateStructGEP(type, alloc, 0, "tag"));

      if (getOutermostAstTag(ast->children[2]) == "exprlist") {
        const auto exprList = getExprList(ast->children[2]);
        if (exprList.size() != type->getStructNumElements() - 1) {
          return error("invalid number of elements for constructor "
                       "`{}` of data class `{}` at line {}",
                       ctorName, className, ast->state.row + 1);
        }

        for (size_t i = 0; i < exprList.size(); ++i) {
          auto val = exprList[i]->codegen(builder);
          if (!val) {
            return val.takeError();
          }
          const auto value = *val;
          const auto ptr = builder.CreateStructGEP(type, alloc, i + 1, "");

          if (value->getType() != ptr->getType()->getPointerElementType()) {
            return error("type mismatch at index {} of constructor "
                         "`{}` of data class `{}` at line {}",
                         i, ctorName, className, ast->state.row + 1);
          }
          builder.CreateStore(value, ptr);
        }
      }
      return alloc;
    }

    return error("could not find data class by name `{}` "
                 "at line {}",
                 className, ast->state.row + 1);
  }

  inline static std::optional<unsigned>
  getIndex(const llvm::Module* const module, llvm::StringRef className,
           llvm::StringRef ctorName) {
    return getMetadataPartIndex(*module, "classes", className, ctorName);
  }

private:
  const mpc_state_t state_;
  const std::string class_;
  small_vector<std::pair<llvm::StringRef, std::optional<TypeList>>> variants_;
};

} // end namespace whack::ast

#endif // WHACK_DATACLASS_HPP
