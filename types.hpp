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
#include <llvm/ADT/StringMap.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

namespace whack {

#define CAST_TYPE(...) reinterpret_cast<llvm::Type *>((__VA_ARGS__))

inline static /*const*/ llvm::StringMap<llvm::Type *> BasicTypes{
    {"void", CAST_TYPE(LLVMVoidType())},
     {"bool", CAST_TYPE(LLVMInt1Type())},
     {"char", CAST_TYPE(LLVMInt8Type())},
     {"int", CAST_TYPE(LLVMInt32Type())},
     {"int64", CAST_TYPE(LLVMInt64Type())},
     {"half", CAST_TYPE(LLVMHalfType())},
     {"double", CAST_TYPE(LLVMDoubleType())},
     {"float", CAST_TYPE(LLVMFloatType())},
     {"auto", llvm::cast<llvm::Type>(llvm::StructType::create(
                  *reinterpret_cast<llvm::LLVMContext *>(
                      LLVMGetGlobalContext())))}}; // placeholder

#undef CAST_TYPE

} // end namespace whack

#endif // WHACK_TYPES_HPP
