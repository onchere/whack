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
#ifndef WHACK_MODULE_HPP
#define WHACK_MODULE_HPP

#pragma once

#include "ast/asts.hpp"
#include "parser.hpp"
#include "pass/ctor.hpp"
#include <folly/Memory.h>
#include <folly/ScopeGuard.h>
#include <llvm-c/Initialization.h>
#include <llvm-c/Linker.h>
#include <llvm-c/Support.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>

namespace whack {

class Module {
  using ast_t = std::unique_ptr<
      mpc_ast_t, folly::static_function_deleter<mpc_ast_t, &mpc_ast_delete>>;

public:
  // @todo
  explicit Module(const Parser& parser, const std::string& sourceFileName) {
    mpc_result_t res;
    if (!mpc_parse_contents(sourceFileName.c_str(), parser.get(), &res)) {
      mpc_err_print(res.error);
      mpc_err_delete(res.error);
    } else {
      ast_ = ast_t{reinterpret_cast<mpc_ast_t*>(res.output)};
      this->init();
      this->traverse(ast_.get());
    }
  }

  // @todo
  explicit Module(const std::string& grammarFileName,
                  const std::string& sourceFileName)
      : Module(Parser{grammarFileName}, sourceFileName) {}

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  llvm::Expected<int> run(const int argc, const char* const argv[],
                          const char* const env[] = nullptr) {
    try {
      auto module = this->codegen();
      if (!module) {
        return module.takeError();
      }
      // @todo Link in runtime.o
      llvm::EngineBuilder builder{std::move(*module)};
      builder.setEngineKind(llvm::EngineKind::JIT);
      builder.setVerifyModules(true);
      builder.setUseOrcMCJITReplacement(false);
      std::unique_ptr<llvm::ExecutionEngine> eng{builder.create(
          reinterpret_cast<llvm::TargetMachine*>(targetMachine_))};
      std::vector<std::string> args;
      for (auto i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
      }
      return eng->runFunctionAsMain(eng->FindFunctionNamed("wain"), args, env);
    } catch (const std::exception& e) {
      return error(e.what());
    }
  }

  llvm::Error compile(const std::string& objFileName = "main.o",
                      const std::string& execFileName = "main",
                      const std::string& linkerArgs = "") {
    auto mod = this->codegen();
    if (!mod) {
      return mod.takeError();
    }
    const auto module = std::move(*mod);
    module->dump();
    char* err;
    if (LLVMTargetMachineEmitToFile(targetMachine_, llvm::wrap(module.get()),
                                    const_cast<char*>(objFileName.data()),
                                    LLVMObjectFile, &err)) {
      auto ret = error(std::string{err});
      LLVMDisposeMessage(err);
      return ret;
    } else {
      if (llvm::sys::findProgramByName("gcc")) {
        const auto command =
            format("gcc runtime.o {} -o {}.exe", objFileName, execFileName);
        system(command.c_str());
        return llvm::Error::success(); // @todo llvm::sys::ExecuteAndWait
      } else {
        return error("could not find `gcc` on your system PATH. "
                     "Please find MinGW GCC at "
                     "https://nuwen.net and install.");
      }
    }
  }

private:
  llvm::LLVMContext context_;
  llvm::legacy::PassManager passManager_;
  ast_t ast_;
  LLVMTargetMachineRef targetMachine_;
  small_vector<ast::CompilerOpt> compilerOpts_;
  std::unique_ptr<ast::ModuleDecl> moduleDecl_;
  small_vector<ast::ModuleUse> moduleUse_;
  small_vector<ast::Exports> exports_;
  // @todo use variant<>, call codegen with std::visit
  small_vector<std::unique_ptr<const ast::AST>> elements_;

  void init() {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    LLVMInitializeNativeTarget();
    // @todo Link in MCJIT for interpreter

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

    passManager_.add(new pass::Ctor);
  }

  void traverse(mpc_ast_t* const ast) {
    auto astTravel = mpc_ast_traverse_start(ast, mpc_ast_trav_order_pre);
    SCOPE_EXIT { mpc_ast_traverse_free(&astTravel); };
    while (const auto current = mpc_ast_traverse_next(&astTravel)) {
      using namespace whack::ast;
      const std::string_view tag{current->tag};
      if (tag == "moduledecl|>") {
        moduleDecl_ = std::make_unique<ModuleDecl>(current);
      } else if (tag == "moduleuse|>") {
        moduleUse_.emplace_back(ModuleUse{current});
      } else if (tag == "exports|>") {
        exports_.emplace_back(Exports{current});
      }
#define OPT(TAG, CLASS)                                                        \
  else if (tag == TAG) {                                                       \
    elements_.emplace_back(std::make_unique<CLASS>(current));                  \
  }
      OPT("compileropt|>", CompilerOpt) // @todo impl as Metadata
      OPT("externfunc|>", ExternFunc)
      OPT("interface|>", Interface)
      OPT("enumeration|>", Enumeration)
      OPT("function|>", Function)
      OPT("structure|>", Structure)
      OPT("alias|>", Alias)
      OPT("structfunc|>", StructFunc)
      OPT("structop|>", StructOp)
      OPT("dataclass|>", DataClass)
#undef OPT
    }
  }

  llvm::Expected<std::string> resolveModulePath(llvm::StringRef moduleName) {
    // @todo
    return "";
  }

  llvm::Expected<std::unique_ptr<llvm::Module>> codegen() {
    if (!ast_) {
      return error("Invalid AST");
    }
    auto module = std::make_unique<llvm::Module>(moduleDecl_->name(), context_);
    // @todo Link in loaded modules; mangle symbols according to exports table

#define OPT(CLASS)                                                             \
  if (const auto elt = dynamic_cast<const ast::CLASS*>(element.get())) {       \
    if (auto err = elt->codegen(module.get())) {                               \
      return err;                                                              \
    }                                                                          \
    continue; /*we've already matched*/                                        \
  }
    for (const auto& element : elements_) {
      OPT(Alias)
      OPT(ExternFunc)
      OPT(DataClass)
      OPT(Interface)
      OPT(Enumeration)
      OPT(Structure) OPT(StructFunc) OPT(StructOp) OPT(Function)
    }
#undef OPT

    passManager_.run(*module);

    return module;
  }
};

} // end namespace whack

#endif // WHACK_MODULE_HPP
