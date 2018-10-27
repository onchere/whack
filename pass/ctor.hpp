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
#ifndef WHACK_PASSES_CTOR_HPP
#define WHACK_PASSES_CTOR_HPP

#pragma once

#include "../ast/metadata.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/Pass.h>

namespace whack::pass {

struct Ctor : public llvm::FunctionPass {
  char pid = getpid();
  Ctor() : llvm::FunctionPass(pid) {}
  bool runOnFunction(llvm::Function& func) override {
    const auto& module = *func.getParent();
    llvm::IRBuilder<> builder{func.getContext()};
    const auto __ctor = [&](const llvm::Instruction& inst) {
      const auto type = inst.getType()->getPointerElementType();
      if (type->isStructTy()) {
        const auto structName = type->getStructName().str();
        // do we really have a "struct"? // check redudant??
        if (ast::getMetadataOperand(module, "structures", structName)) {
          // @todo: Handle overloaded (mangled) constructors
          const auto funcName = format("struct::{}::{}", structName, "__ctor");
          if (const auto ctor = module.getFunction(funcName)) {
            auto i = const_cast<llvm::Instruction*>(&inst);
            builder.SetInsertPoint(&*++i->getIterator());
            builder.CreateCall(ctor, llvm::dyn_cast<llvm::Value>(i));
          }
        }
      } else {
        // @todo
      }
    };

    for (const auto& block : func) {
      for (const auto& inst : block) {
        if (llvm::isa<llvm::AllocaInst>(inst)) {
          __ctor(inst);
        }
      }
    }

    // we also call ctor over "new" struct allocas
    const auto news =
        ast::getAllMetadataOperands(module, "new", func.getName());
    if (!news.empty()) {
      const auto symTbl = func.getValueSymbolTable();
      for (const auto& operand : news) {
        for (unsigned j = 1; j < operand->getNumOperands(); j += 2) {
          const auto var =
              reinterpret_cast<llvm::MDString*>(operand->getOperand(j).get())
                  ->getString();
          // for a "new" variable, the next instruction is always a store for
          // 	malloc'd memory to the variable
          // we run the constructor over the memory before storing the pointer
          // 	in var
          const auto alloc = symTbl->lookup(var);
          for (const auto& block : func) {
            for (const auto& inst : block) {
              if (llvm::dyn_cast<llvm::Value>(&inst) == alloc) {
                __ctor(*llvm::dyn_cast<llvm::Instruction>(
                    (*++inst.getIterator()).getOperand(0)));
              }
            }
          }
        }
      }
    }
    return true;
  }
};

} // namespace whack::pass

#endif // WHACK_PASSES_CTOR_HPP
