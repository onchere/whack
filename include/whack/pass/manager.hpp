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
#ifndef WHACK_PASSES_MANAGER_HPP
#define WHACK_PASSES_MANAGER_HPP

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Coroutines.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

enum OptLevel { d, O1, O2, O3 };
enum SizeOptLevel { O0, Os, Oz };

extern OptLevel OptimizationLevel;
extern SizeOptLevel SizeOptimizationLevel;

namespace whack::pass {

class Manager {
public:
  Manager() {
    llvm::PassManagerBuilder passManagerBuilder;
    passManagerBuilder.OptLevel = OptimizationLevel;
    passManagerBuilder.SizeLevel = SizeOptimizationLevel;
    // llvm::addCoroutinePassesToExtensionPoints(passManagerBuilder);
    passManagerBuilder.populateModulePassManager(passManager_);
  }

  inline bool run(llvm::Module& module) {
    return passManager_.run(module);
  }

  inline bool run(llvm::Module* const module) {
    return this->run(*module);
  }

private:
  llvm::legacy::PassManager passManager_;
};

} // end namespace whack::pass

#endif // WHACK_PASSES_MANAGER_HPP
