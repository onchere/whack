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
#ifndef WHACK_TYPES_HPP
#define WHACK_TYPES_HPP

#pragma once

#include <llvm-c/Core.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

namespace whack {

static llvm::Type* const getBasicType(const llvm::StringRef typeName) {
  using namespace llvm;
  auto& ctx = *reinterpret_cast<LLVMContext*>(LLVMGetGlobalContext());
  return StringSwitch<llvm::Type*>(typeName)
      .Case("void", Type::getVoidTy(ctx))
      .Case("bool", Type::getInt1Ty(ctx))
      .Case("char", Type::getInt8Ty(ctx))
      .Case("int8", Type::getInt8Ty(ctx))
      .Case("uint8", Type::getInt8Ty(ctx))
      .Case("short", Type::getInt16Ty(ctx))
      .Case("int16", Type::getInt16Ty(ctx))
      .Case("uint16", Type::getInt16Ty(ctx))
      .Case("int", Type::getInt32Ty(ctx))
      .Case("int32", Type::getInt32Ty(ctx))
      .Case("uint", Type::getInt32Ty(ctx))
      .Case("uint32", Type::getInt32Ty(ctx))
      .Case("int64", Type::getInt64Ty(ctx))
      .Case("uint64", Type::getInt64Ty(ctx))
      .Case("int128", Type::getInt128Ty(ctx))
      .Case("uint128", Type::getInt128Ty(ctx))
      .Case("half", Type::getHalfTy(ctx))
      .Case("double", Type::getDoubleTy(ctx))
      .Case("float", Type::getFloatTy(ctx))
      .Case("auto", StructType::get(ctx, "auto")) // placeholder
      .Default(nullptr);
}

static struct BasicTypes {
  inline auto operator[](const llvm::StringRef typeName) const {
    return getBasicType(typeName);
  }
} BasicTypes;

// @todo References?
static auto getTypeName(llvm::Type* type) {
  size_t numPointers = 0;
  while (type->isPointerTy()) {
    ++numPointers;
    type = type->getPointerElementType();
  }
  std::string typeName;
  llvm::raw_string_ostream os{typeName};
  if (!type->isPointerTy()) {
    // @todo Signed integers, enums, data classes, functions?
    if (type->isIntegerTy()) {
      switch (type->getPrimitiveSizeInBits()) {
      case 1:
        os << "bool";
        break;
      case 8:
        os << "char";
        break;
      case 16:
        os << "short";
        break;
      case 32:
        os << "int";
        break;
      case 64:
        os << "int64";
        break;
      case 128:
        os << "int128";
        break;
      }
    } else if (type->isFloatingPointTy()) {
      switch (type->getPrimitiveSizeInBits()) {
      case 16:
        os << "half";
        break;
      case 32:
        os << "float";
        break;
      case 64:
        os << "double";
        break;
      }
    } else if (type->isStructTy()) {
      const auto name = type->getStructName();
      if (name.startswith("interface::")) {
        constexpr static auto nameOffset = std::strlen("interface::");
        os << name.substr(nameOffset);
      } else {
        os << name;
      }
    } else {
      // @todo Do/Should we even get here?
      llvm::errs() << "unhandled type kind in getTypeName";
    }
  }
  while (numPointers--) {
    os << '*';
  }
  os.flush();
  return typeName;
}

} // end namespace whack

#endif // WHACK_TYPES_HPP
