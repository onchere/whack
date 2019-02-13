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
#ifndef WHACK_TARGET_HPP
#define WHACK_TARGET_HPP

#pragma once

#include <folly/ScopeGuard.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm/Support/raw_ostream.h>

namespace whack {

class Target {
public:
  explicit Target(const bool shouldLinkInMCJIT = false) {
    init(shouldLinkInMCJIT);
    auto targetTriple = LLVMGetDefaultTargetTriple();
    SCOPE_EXIT { LLVMDisposeMessage(targetTriple); };
    LLVMTargetRef target;
    char* error;
    if (LLVMGetTargetFromTriple(targetTriple, &target, &error)) {
      llvm::errs() << error;
      LLVMDisposeMessage(error);
    } else {
      assert(LLVMTargetHasJIT(target));
      targetMachine_ = LLVMCreateTargetMachine(
          target, targetTriple, "", "", LLVMCodeGenLevelDefault,
          LLVMRelocDefault, LLVMCodeModelJITDefault);
      assert(targetMachine_);
    }
  }

  inline const auto getMachine() const { return targetMachine_; }

  ~Target() {
    if (targetMachine_) {
      LLVMDisposeTargetMachine(targetMachine_);
    }
  }

private:
  LLVMTargetMachineRef targetMachine_;

  static void init(const bool shouldLinkInMCJIT) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    LLVMInitializeNativeTarget();
    if (shouldLinkInMCJIT) {
      LLVMLinkInMCJIT();
    }
  }
};

} // end namespace whack

#endif // WHACK_TARGET_HPP
