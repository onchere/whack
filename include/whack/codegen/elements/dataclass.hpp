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
#ifndef WHACK_DATACLASS_HPP
#define WHACK_DATACLASS_HPP

#pragma once

#include "../expressions/factors/character.hpp"
#include "../expressions/factors/ident.hpp"
#include "../expressions/factors/scoperes.hpp"
#include "../types/typelist.hpp"
#include <folly/Likely.h>
#include <llvm/Support/raw_ostream.h>

namespace whack::codegen::elements {

class DataClass final : public AST {
public:
  explicit DataClass(const mpc_ast_t* const ast)
      : state_{ast->state}, class_{ast->children[1]->contents} {
    const auto ref = ast->children[2];
    for (auto i = 2; i < ref->children_num - 4; i += 4) {
      const auto name = ref->children[i]->contents;
      if (getOutermostAstTag(ref->children[i + 2]) == "typelist") {
        types::TypeList typeList{ref->children[i + 2]};
        variants_.emplace_back(
            std::pair{name, std::optional{std::move(typeList)}});
        ++i;
      } else {
        variants_.emplace_back(std::pair{name, std::nullopt});
      }
    }
  }

  // registers a data class type
  llvm::Error codegen(llvm::Module* const module) const {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(module, class_)) {
      return err;
    }
    auto& ctx = module->getContext();
    llvm::MDBuilder MDBuilder{ctx};
    unsigned int biggestSize = 1;
    const auto dataClass = llvm::StructType::create(ctx, "class::" + class_);
    small_vector<std::pair<llvm::MDNode*, uint64_t>> metadata;
    llvm::IRBuilder<> builder{module->getContext()};
    for (const auto& [name, typeList] : variants_) {
      const auto classMD =
          reinterpret_cast<llvm::MDNode*>(MDBuilder.createString(name));
      metadata.emplace_back(std::pair{classMD, metadata.size()});
      const auto className = format("class::{}::{}", class_, name);
      if (typeList) {
        auto t = typeList.value().codegen(builder);
        if (!t) {
          return t.takeError();
        }
        auto [types, variadic] = std::move(*t);
        if (variadic) {
          return error("cannot use variadic type in typelist for "
                       "constructor `{}` in data class `{}` "
                       "at line {}",
                       name.data(), class_, state_.row + 1);
        }
        types.insert(types.begin(), BasicTypes["char"]); // for tag
        const auto variant = llvm::StructType::create(ctx, types, className);
        const auto size = types::Type::getBitSize(module, variant);
        if (size > biggestSize) {
          biggestSize = size;
        }
      } else {
        (void)llvm::StructType::create(ctx, BasicTypes["char"] /*tag*/,
                                       className);
      }
    }

    llvm::Type* storeType; // @todo
    if (biggestSize <= 8) {
      storeType = llvm::Type::getIntNTy(ctx, biggestSize);
    } else {
      storeType = llvm::ArrayType::get(BasicTypes["char"], biggestSize / 8);
    }

    dataClass->setBody({BasicTypes["char"], storeType});
    auto MD = module->getOrInsertNamedMetadata("classes");
    MD->addOperand(MDBuilder.createTBAAStructTypeNode(class_, metadata));
    return llvm::Error::success();
  }

  static bool isa(const mpc_ast_t* const ast,
                  const llvm::Module* const module) {
    if (getInnermostAstTag(ast) != "scoperes") {
      return false;
    }
    const auto [className, _] = getDataClassInfo(ast);
    const auto dataClass = format("class::{}", std::move(className));
    return module->getTypeByName(dataClass) != nullptr;
  }

  /// @brief constructs a value by selecting a data class ctor
  /// Assumes we have a data class in @param ast
  /// It's up to the caller to ensure we really do
  static llvm::Expected<llvm::Value*> construct(const mpc_ast_t* const ast,
                                                llvm::IRBuilder<>& builder) {
    const auto [className, ctorName] = getDataClassInfo(ast);
    const auto dataClass = format("class::{}", className);
    const auto module = builder.GetInsertBlock()->getModule();
    const auto dataClassType = module->getTypeByName(dataClass);

    const auto alloc =
        builder.CreateAlloca(dataClassType, 0, nullptr, ctorName);
    const auto idx = DataClass::getIndex(module, className, ctorName);
    if (!idx) {
      return error("could not find constructor `{}` for data "
                   "class `{}` at line {}",
                   ctorName, className, ast->state.row + 1);
    }
    using namespace expressions::factors;
    const auto tag = Character::get(idx.value());
    builder.CreateStore(
        tag, builder.CreateStructGEP(dataClassType, alloc, 0, "tag"));

    if (getOutermostAstTag(ast->children[2]) == "exprlist") {
      const auto exprList = expressions::getExprList(ast->children[2]);
      const auto variantType =
          module->getTypeByName(format("class::{}::{}", className, ctorName));
      if (exprList.size() != variantType->getStructNumElements() - 1) {
        return error("invalid number of elements for constructor "
                     "`{}` of data class `{}` at line {}",
                     ctorName, className, ast->state.row + 1);
      }
      const auto variant =
          builder.CreateBitCast(alloc, variantType->getPointerTo(0));
      for (size_t i = 0; i < exprList.size(); ++i) {
        auto val = exprList[i]->codegen(builder);
        if (!val) {
          return val.takeError();
        }
        const auto value = *val;
        const auto ptr =
            builder.CreateStructGEP(variantType, variant, i + 1, "");
        if (value->getType() != ptr->getType()->getPointerElementType()) {
          return error("type mismatch at index {} of constructor "
                       "`{}` of data class `{}` at line {}",
                       i, ctorName, className, ast->state.row + 1);
        }
        builder.CreateStore(value, ptr);
      }
    }
    return builder.CreateLoad(alloc);
  }

  inline static std::optional<unsigned>
  getIndex(const llvm::Module* const module, llvm::StringRef className,
           llvm::StringRef ctorName) {
    return getMetadataPartIndex(*module, "classes", className, ctorName);
  }

private:
  friend class DataClassStmt;
  const mpc_state_t state_;
  const std::string class_;
  std::vector<std::pair<std::string, std::optional<types::TypeList>>> variants_;

  static std::pair<std::string, std::string>
  getDataClassInfo(const mpc_ast_t* const ast) {
    auto parts = expressions::factors::ScopeRes{ast}.parts();
    auto ctorName = parts.back().str();
    parts.pop_back();
    std::string className;
    llvm::raw_string_ostream os{className};
    for (size_t i = 0; i < parts.size(); ++i) {
      os << parts[i];
      if (i < parts.size() - 1) {
        os << "::";
      }
    }
    os.flush();
    return {std::move(className), std::move(ctorName)};
  }
};

class DataClassStmt final : public stmts::Stmt {
public:
  explicit DataClassStmt(const mpc_ast_t* const ast)
      : Stmt(kDataClass), impl_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    using namespace expressions::factors;
    if (auto err = Ident::isUnique(builder, impl_.class_)) {
      return err;
    }
    return impl_.codegen(builder.GetInsertBlock()->getModule());
  }

  llvm::Error runScopeExit(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    const auto baseName = format("class::{}", impl_.class_);
    const auto variantBase = module->getTypeByName(baseName);
    if (UNLIKELY(variantBase == nullptr)) {
      return llvm::Error::success();
    }
    variantBase->setName(".tmp." + variantBase->getName().str());
    renameMetadataOperand(*module, "classes", impl_.class_,
                          variantBase->getName());
    const auto newBaseName = variantBase->getName();
    for (const auto& [name, _] : impl_.variants_) {
      const auto structure =
          module->getTypeByName(format("class::{}::{}", impl_.class_, name));
      if (LIKELY(structure != nullptr)) {
        structure->setName(format("{}::{}", newBaseName.data(), name));
      }
    }
    return llvm::Error::success();
  }

private:
  const DataClass impl_;
};

} // namespace whack::codegen::elements

#endif // WHACK_DATACLASS_HPP
