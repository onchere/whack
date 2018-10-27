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
#ifndef WHACK_METADATA_HPP
#define WHACK_METADATA_HPP

#include <llvm/IR/IRBuilder.h>

#pragma once

namespace whack::ast {

static const auto getAllMetadataOperands(const llvm::Module& module,
                                         llvm::StringRef metadata,
                                         llvm::StringRef name) {
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

inline static const std::optional<llvm::MDNode*>
getMetadataOperand(const llvm::Module& module, llvm::StringRef metadata,
                   llvm::StringRef name) {
  const auto operands = getAllMetadataOperands(module, metadata, name);
  if (operands.empty()) {
    return std::nullopt;
  }
  return operands.front();
}

static const auto getMetadataParts(const llvm::Module& module,
                                   llvm::StringRef metadata) {
  small_vector<llvm::StringRef> parts;
  const auto MD = module.getNamedMetadata(metadata);
  if (!MD) {
    return parts;
  }
  for (unsigned i = 0; i < MD->getNumOperands(); ++i) {
    parts.push_back(
        reinterpret_cast<llvm::MDString*>(MD->getOperand(i))->getString());
  }
  return parts;
}

static const auto getMetadataParts(const llvm::Module& module,
                                   llvm::StringRef metadata,
                                   llvm::StringRef name) {
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
getMetadataPartIndex(const llvm::Module& module, llvm::StringRef metadata,
                     llvm::StringRef name, llvm::StringRef part) {
  const auto parts = getMetadataParts(module, metadata, name);
  for (size_t i = 0; i < parts.size(); ++i) {
    if (parts[i] == part) {
      return i;
    }
  }
  return std::nullopt;
}

} // end namespace whack::ast

#endif // WHACK_METADATA_HPP
