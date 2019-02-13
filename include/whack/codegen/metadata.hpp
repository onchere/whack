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
#ifndef WHACK_METADATA_HPP
#define WHACK_METADATA_HPP

#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/MDBuilder.h>

namespace whack::codegen {

static const bool hasMetadata(llvm::Value* const value,
                              const int pinnedMetadataKind) {
  if (!llvm::isa<llvm::Instruction>(value)) {
    return false;
  }
  return llvm::cast<llvm::Instruction>(value)->getMetadata(
             pinnedMetadataKind) != nullptr;
}

static void setIsDereferenceable(llvm::LLVMContext& ctx,
                                 llvm::Value* const value) {
  if (!llvm::isa<llvm::Instruction>(value)) {
    return;
  }
  llvm::cast<llvm::Instruction>(value)->setMetadata(
      llvm::LLVMContext::MD_dereferenceable,
      llvm::MDBuilder{ctx}.createTBAARoot("::dereferenceable"));
}

static const auto getAllMetadataOperands(const llvm::Module& module,
                                         const llvm::StringRef metadata,
                                         const llvm::StringRef name) {
  small_vector<llvm::MDNode*> operands;
  const auto MD = module.getNamedMetadata(metadata);
  if (!MD) {
    return operands;
  }
  for (unsigned i = 0; i < MD->getNumOperands(); ++i) {
    const auto operand = MD->getOperand(i);
    const auto str =
        reinterpret_cast<llvm::MDString*>(operand->getOperand(0).get())
            ->getString();
    if (name == str) {
      operands.push_back(operand);
    }
  }
  return operands;
}

static const std::optional<llvm::MDNode*>
getMetadataOperand(const llvm::Module& module, const llvm::StringRef metadata,
                   const llvm::StringRef name) {
  const auto operands = getAllMetadataOperands(module, metadata, name);
  if (operands.empty()) {
    return std::nullopt;
  }
  return operands.front();
}

template <size_t Index = 0>
static void renameMetadataOperand(llvm::Module& module,
                                  const llvm::StringRef metadata,
                                  const llvm::StringRef operandName,
                                  const llvm::StringRef newOperandName) {
  if (auto MD = getMetadataOperand(module, metadata, operandName)) {
    llvm::MDBuilder MDBuilder{module.getContext()};
    MD.value()->replaceOperandWith(Index,
                                   MDBuilder.createString(newOperandName));
  }
}

template <size_t OperandDepth = 0>
static const auto getMetadataParts(const llvm::Module& module,
                                   const llvm::StringRef metadata) {
  static_assert(OperandDepth < 2, "not implemented for OperandDepth > 1");
  small_vector<llvm::StringRef> parts;
  const auto MD = module.getNamedMetadata(metadata);
  if (!MD) {
    return parts;
  }
  for (unsigned i = 0; i < MD->getNumOperands(); ++i) {
    if constexpr (OperandDepth == 0) {
      parts.push_back(
          reinterpret_cast<llvm::MDString*>(MD->getOperand(i))->getString());
    } else if constexpr (OperandDepth == 1) {
      const auto& str = MD->getOperand(i)->getOperand(0);
      parts.push_back(
          reinterpret_cast<llvm::MDString*>(str.get())->getString());
    }
  }
  return parts;
}

static const auto getMetadataParts(const llvm::Module& module,
                                   const llvm::StringRef metadata,
                                   const llvm::StringRef name) {
  small_vector<llvm::StringRef> parts;
  if (const auto op = getMetadataOperand(module, metadata, name)) {
    const auto operand = op.value();
    for (unsigned j = 1; j < operand->getNumOperands(); j += 2) {
      parts.push_back(
          reinterpret_cast<llvm::MDString*>(operand->getOperand(j).get())
              ->getString());
    }
  }
  return parts;
}

static const std::optional<size_t>
getMetadataPartIndex(const llvm::Module& module, const llvm::StringRef metadata,
                     const llvm::StringRef name, const llvm::StringRef part) {
  const auto parts = getMetadataParts(module, metadata, name);
  for (size_t i = 0; i < parts.size(); ++i) {
    if (parts[i] == part) {
      return i;
    }
  }
  return std::nullopt;
}

template <typename T>
static typename std::enable_if_t<std::is_convertible_v<T, llvm::StringRef>>
addStructTypeMetadata(llvm::Module* const module, const llvm::StringRef MDName,
                      const llvm::StringRef name,
                      const small_vector<T>& fields) {
  std::vector<std::pair<llvm::MDNode*, uint64_t>> metadata;
  llvm::MDBuilder MDBuilder{module->getContext()};
  for (const auto& field : fields) {
    const auto MD =
        reinterpret_cast<llvm::MDNode*>(MDBuilder.createString(field));
    metadata.emplace_back(std::pair{MD, metadata.size()});
  }
  const auto structMD = MDBuilder.createTBAAStructTypeNode(name, metadata);
  module->getOrInsertNamedMetadata(MDName)->addOperand(structMD);
}

} // end namespace whack::codegen

#endif // WHACK_METADATA_HPP
