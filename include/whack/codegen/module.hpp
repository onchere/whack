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
#ifndef WHACK_MODULE_HPP
#define WHACK_MODULE_HPP

#pragma once

#include "../parser.hpp"
#include "../pass/manager.hpp"
#include "../target.hpp"
#include "elements/element.hpp"
#include "metadata.hpp"
#include <folly/Likely.h>
#include <folly/Memory.h>
#include <folly/ScopeGuard.h>
#include <lib/IR/LLVMContextImpl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Module.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/Program.h>

extern std::string InputFilename;
extern std::string GrammarFilename;
extern std::string OutputObjectFilename;
extern std::string OutputExecutableFilename;
extern bool EmitLLVM;
extern std::vector<std::string> ModuleSearchPaths;

namespace whack::codegen {

struct ParserCreator {
  inline static void* call() { return new Parser{GrammarFilename}; }
};

static llvm::ManagedStatic<Parser, ParserCreator> MainParser;
static llvm::ManagedStatic<Target> MainTarget;
static llvm::ManagedStatic<pass::Manager> PassManager;

class Module {
  using ast_t = std::unique_ptr<
      mpc_ast_t, folly::static_function_deleter<mpc_ast_t, &mpc_ast_delete>>;

public:
  explicit Module(const std::string& inputFileName)
      : ownsContext_{true}, context_{new llvm::LLVMContext} {
    this->init(inputFileName);
  }

  explicit Module(const std::string& inputFileName,
                  llvm::LLVMContext* const context)
      : ownsContext_{false}, context_{context} {
    this->init(inputFileName);
  }

  Module() : Module(InputFilename) {}

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;

  llvm::Expected<std::unique_ptr<llvm::Module>> codegen() {
    if (!ast_) {
      return error("invalid translation unit");
    }
    auto module = std::make_unique<llvm::Module>(moduleName_, *context_);
    if (auto err = this->fill(module.get())) {
      return err;
    }
    return module;
  }

  llvm::Error compile() {
    auto mod = this->codegen();
    if (!mod) {
      return mod.takeError();
    }
    const auto module = std::move(*mod);
    if (EmitLLVM) {
      return this->emitLLVMIR(module.get());
    }
    module->dump();
    if (auto err = this->emitObjectFile(module.get())) {
      return err;
    }
    return this->link();
  }

  // @todo Test
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
          reinterpret_cast<llvm::TargetMachine*>(MainTarget->getMachine()))};
      std::vector<std::string> args;
      for (auto i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
      }
      // User should use "wain" as entry point instead of main?
      return eng->runFunctionAsMain(eng->FindFunctionNamed("wain"), args, env);
    } catch (const std::exception& e) {
      return error(e.what());
    }
  }

  inline const auto& name() const { return moduleName_; }

  ~Module() {
    if (ownsContext_ && context_ != nullptr) {
      delete context_;
    }
  }

private:
  const bool ownsContext_;
  llvm::LLVMContext* const context_;
  ast_t ast_;
  llvm::StringRef moduleName_;
  // Should be useful when we implement macros
  std::vector<elements::element_t> elements_;

  void init(const std::string& inputFileName) {
    mpc_result_t res;
    if (!mpc_parse_contents(inputFileName.c_str(), MainParser->get(), &res)) {
      mpc_err_print(res.error);
      mpc_err_delete(res.error);
    } else {
      ast_ = ast_t{reinterpret_cast<mpc_ast_t*>(res.output)};
      this->traverse(ast_.get());
    }
  }

  void traverse(mpc_ast_t* const ast) {
    auto astTravel = mpc_ast_traverse_start(ast, mpc_ast_trav_order_pre);
    SCOPE_EXIT { mpc_ast_traverse_free(&astTravel); };
    while (const auto current = mpc_ast_traverse_next(&astTravel)) {
      using namespace elements;
      const std::string_view tag{current->tag};
      if (tag == "moduledecl|>") {
        moduleName_ = current->children[1]->contents;
      }
#define OPT(TAG, CLASS)                                                        \
  else if (tag == TAG) {                                                       \
    elements_.emplace_back(CLASS{current});                                    \
  }
      OPT("comment|>", Comment)
      OPT("compileropt|>", CompilerOpt)
      OPT("moduleuse|>", ModuleUse)
      OPT("exports|>", Exports)
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

  llvm::Error fill(llvm::Module* const module) {
    llvm::Error err = llvm::Error::success();
    for (const auto& elem : elements_) {
      std::visit(
          [module, &err](auto&& element) {
            if (auto e = element.codegen(module)) {
              err = err ? llvm::joinErrors(std::move(err), std::move(e))
                        : std::move(e);
            }
          },
          elem);
    }
    if (err) {
      return err;
    }
    // We only run opt passes on the Main module @todo
    if (module->getModuleIdentifier() == "Main") {
      PassManager->run(module);
    }
    return llvm::Error::success();
  }

  llvm::Error emitLLVMIR(const llvm::Module* const module) {
    std::error_code ec;
    llvm::raw_fd_ostream os{module->getModuleIdentifier() + ".ll", ec,
                            llvm::sys::fs::OpenFlags::F_RW};
    if (ec) {
      return error("error emitting LLVM IR: {}", ec.message());
    }
    module->print(os, nullptr);
    os.close();
    return llvm::Error::success();
  }

  llvm::Error emitObjectFile(const llvm::Module* const module) {
    char* err;
    const auto e = LLVMTargetMachineEmitToFile(
        MainTarget->getMachine(), llvm::wrap(module),
        const_cast<char*>(OutputObjectFilename.c_str()), LLVMObjectFile, &err);
    if (e) {
      auto ret = error(err);
      LLVMDisposeMessage(err);
      return ret;
    }
    return llvm::Error::success();
  }

  llvm::Error link() {
    if (!llvm::sys::findProgramByName("gcc")) {
      return error("Could not find `gcc` on your system PATH. "
                   "Please find MinGW GCC at "
                   "https://nuwen.net and install.");
    }
    const auto command = format("gcc runtime.o {} -o {}", OutputObjectFilename,
                                OutputExecutableFilename);
    system(command.c_str()); // @todo llvm::sys::ExecuteAndWait
    return llvm::Error::success();
  }
};

static llvm::Error
importModuleImpl(llvm::Module* const destModule,
                 std::unique_ptr<llvm::Module> importedModule,
                 const ModuleImportInfo importInfo,
                 const bool ignoreConflicts) {
  const auto isHidden = [&importInfo](const llvm::StringRef name) -> bool {
    if (!importInfo.elements) {
      return false;
    }
    const auto& elems = importInfo.elements.value();
    const auto end = elems.end();
    const auto iter = std::find(elems.begin(), end, name);
    return importInfo.hiding ? iter != end : iter == end;
  };

  const auto srcModule = importedModule.get();
  auto exports = elements::Exports::get(srcModule);
  // We propagate the imports "upwards" the import chain
  for (const auto& import : getMetadataParts<1>(*srcModule, "imports")) {
    exports.push_back(import);
  }

  const auto imported = [&exports,
                         &isHidden](const llvm::StringRef name) -> bool {
    if (std::find(exports.begin(), exports.end(), name) == exports.end()) {
      return false;
    }
    return !isHidden(name);
  };

  const auto& qualifier = importInfo.qualifier;
  const std::string qual =
      qualifier ? format("{}::", qualifier.value().data()) : "";
  const auto& destName = destModule->getModuleIdentifier();
  const auto exists = [destModule,
                       destName](const llvm::StringRef name) -> llvm::Error {
    if (destModule->getValueSymbolTable().lookup(name)) {
      return error("import error: symbol `{}` already exists in module `{}`",
                   name.data(), destName);
    }
    return llvm::Error::success();
  };

  // We import the exports to "propagate visibility upwards"
  const auto importMD = destModule->getOrInsertNamedMetadata("imports");
  llvm::MDBuilder MDBuilder{destModule->getContext()};
  for (const auto& exp : exports) {
    importMD->addOperand(MDBuilder.createTBAARoot(exp));
  }

  const auto importAlias = [&](llvm::GlobalAlias& alias) -> llvm::Error {
    const auto aliasName = alias.getName().data();
    const auto newName = format("{}{}", qual, aliasName);
    if (auto err = exists(newName)) {
      if (ignoreConflicts) {
        llvm::consumeError(std::move(err));
        return llvm::Error::success();
      }
      return err;
    }
    alias.setName(newName);
    // enumerations...
    for (auto& glob : srcModule->globals()) {
      const auto name = glob.getName();
      if (name.startswith(format("{}::", aliasName))) {
        const auto newAliasName = format("{}{}", qual, name.data());
        if (auto err = exists(newAliasName)) {
          if (ignoreConflicts) {
            llvm::consumeError(std::move(err));
            continue;
          }
          return err;
        }
        glob.setName(newAliasName);
      }
    }
    return llvm::Error::success();
  };

  // We import type aliases + enumerations (whose typename is an integral alias)
  for (auto& alias : srcModule->getAliasList()) {
    if (!imported(alias.getName())) {
      continue;
    }
    if (auto err = importAlias(alias)) {
      return err;
    }
  }

  std::unordered_map<std::string, std::string> oldNewStructNames;
  const auto srcName = srcModule->getModuleIdentifier();
  for (const auto& structure :
       srcModule->getContext().pImpl->NamedStructTypes) {
    const auto structName = structure.getKey();
    if (structName.startswith("class::") ||
        structName.startswith("interface::")) {
      continue;
    }
    const auto newName = imported(structName)
                             ? format("{}{}", qual, structName.data())
                             : format(".tmp.{}.{}", srcName, structName.data());
    structure.getValue()->setName(newName);
    renameMetadataOperand(*srcModule, "structures", structName, newName);
    oldNewStructNames[structName.str()] = std::move(newName);
  }

  // We import data classes
  for (const auto& dataClass : getMetadataParts<1>(*srcModule, "classes")) {
    if (!imported(dataClass)) {
      continue;
    }
    // Import base type
    const auto name = format("class::{}{}", qual, dataClass.data());
    if (auto err = exists(name)) {
      if (ignoreConflicts) {
        llvm::consumeError(std::move(err));
        continue;
      }
      return err;
    }
    const auto baseName = format("class::{}", dataClass.data());
    srcModule->getTypeByName(baseName)->setName(name);

    // We also import the variants
    for (const auto& variant :
         getMetadataParts(*srcModule, "classes", dataClass)) {
      const auto newName = format("{}{}", name, variant.data());
      if (auto err = exists(newName)) {
        if (ignoreConflicts) {
          llvm::consumeError(std::move(err));
          continue;
        }
        return err;
      }
      const auto structure =
          srcModule->getTypeByName(format("{}::{}", baseName, variant.data()));
      structure->setName(newName);
    }
    renameMetadataOperand(*srcModule, "classes", dataClass,
                          format("{}{}", qual, dataClass.data()));
  }

  // We import interfaces
  for (const auto& interface : getMetadataParts<1>(*srcModule, "interfaces")) {
    if (!imported(interface)) {
      continue;
    }

    const auto name = format("{}{}", qual, interface.data());
    if (auto err = exists(name)) {
      if (ignoreConflicts) {
        llvm::consumeError(std::move(err));
        continue;
      }
      return err;
    }

    const auto oldName = format("interface::{}", interface.data());
    const auto newName = format("interface::{}", name);
    srcModule->getTypeByName(oldName)->setName(newName);
    renameMetadataOperand(*srcModule, "structures", oldName, newName);
    renameMetadataOperand(*srcModule, "interfaces", interface, name);
  }

  /// We simply rename the symbols in line with import info, we also "mangle"
  /// un-imported symbols to restrict access
  const auto setNewName = [&](llvm::Value* const value) -> llvm::Error {
    const auto name = value->getName();
    const auto newName = format(
        "{}{}", imported(name) ? qual : ".tmp." + srcName + ".", name.data());
    if (auto err = exists(newName)) {
      if (!ignoreConflicts) {
        return err;
      }
      llvm::consumeError(std::move(err));
    }

    if (auto func = llvm::dyn_cast<llvm::Function>(value)) {
      const auto funcType = func->getFunctionType();
      const auto importFuncDecl = [&]() -> llvm::Error {
        if (auto err = exists(name)) {
          if (ignoreConflicts) {
            llvm::consumeError(std::move(err));
          } else {
            return err;
          }
        } else {
          (void)llvm::Function::Create(
              funcType, llvm::Function::ExternalLinkage, name, destModule);
        }
        return llvm::Error::success();
      };

      if (name.startswith("struct::")) {
        for (const auto& [oldName, currentName] : oldNewStructNames) {
          const auto prefix = format("struct::{}::", oldName);
          if (name.startswith(prefix)) {
            const auto newFuncName = format("struct::{}::{}", currentName,
                                            name.substr(prefix.size()).str());
            func->setName(newFuncName);
            break;
          }
        }
      } else if (func->empty()) {
        if (importInfo.qualifier) {
          if (!destModule->getFunction(name)) {
            if (auto err = importFuncDecl()) {
              return err;
            }
          }
          // We redirect qualified extern decls via a qualified-name inline func
          const auto indirectionName = format("{}{}", qual, name.data());
          if (auto err = exists(indirectionName)) {
            if (ignoreConflicts) {
              llvm::consumeError(std::move(err));
              return llvm::Error::success();
            } else {
              return err;
            }
          }
          const auto indirection =
              llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                     indirectionName, destModule);
          const auto entry = llvm::BasicBlock::Create(indirection->getContext(),
                                                      "entry", indirection);
          llvm::IRBuilder<> builder{entry};
          small_vector<llvm::Value*> args;
          for (auto& arg : indirection->args()) {
            args.push_back(&arg);
          }
          const auto call =
              builder.CreateCall(destModule->getFunction(name), args);
          if (funcType->getReturnType() != BasicTypes["void"]) {
            builder.CreateRet(call);
          } else {
            builder.CreateRetVoid();
          }
          indirection->copyAttributesFrom(func);
          indirection->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
        } else {
          if (auto err = importFuncDecl()) {
            return err;
          }
        }
      } else {
        func->setName(newName);
      }
    } else {
      value->setName(newName);
    }
    return llvm::Error::success();
  };

  for (auto& glob : srcModule->globals()) {
    if (auto err = setNewName(&glob)) {
      return err;
    }
  }

  for (auto& func : srcModule->functions()) {
    if (auto err = setNewName(&func)) {
      return err;
    }
  }

  if (llvm::Linker::linkModules(*destModule, std::move(importedModule))) {
    return error("cannot import module `{}` into module `{}`", srcName,
                 destName);
  }
  return llvm::Error::success();
}

/// @brief Imports symbols into destModule using importInfo
/// @todo This import machinery obviously belongs
///   elsewhere, broken up nice into small funcs
static llvm::Error importModule(llvm::Module* const destModule,
                                const ModuleImportInfo importInfo) {
  using namespace llvm::sys;
  llvm::SmallString<1024> thisPath;
  (void)fs::make_absolute(thisPath);
  const auto modulePath = importInfo.modulePath.data();
  auto path = format("{}/{}", thisPath.c_str(), modulePath);
  if (!fs::exists(path)) {
    bool found = false;
    // We look for the first match in provided module search paths
    for (const auto& searchPath : ModuleSearchPaths) {
      const auto tryPath = format("{}/{}", searchPath, modulePath);
      if (fs::exists(tryPath)) {
        path = std::move(tryPath);
        found = true;
        break;
      }
    }
    if (!found) {
      return error("module path `{}` does not exist", modulePath);
    }
  }

  (void)fs::set_current_path(path);
  SCOPE_EXIT { (void)fs::set_current_path(thisPath); };

  std::error_code ec;
  for (auto it = fs::directory_iterator(path, ec);
       it != fs::directory_iterator(); it = it.increment(ec)) {
    if (ec) { // @todo
      ec = {};
      continue;
    }
    const auto& pathEntry = it->path();
    // sub-modules are considered by use::decls.
    if (fs::is_directory(pathEntry)) {
      continue;
    }
    if (!llvm::StringRef{pathEntry}.endswith(".w")) {
      continue;
    }
    Module temp{pathEntry, &destModule->getContext()};
    if (temp.name() != importInfo.moduleName) {
      return error("invalid module name in file at path `{}` "
                   "(expected `{}`, got `{}`)",
                   pathEntry, importInfo.moduleName.data(), temp.name().data());
    }
    auto mod = temp.codegen();
    if (!mod) {
      return mod.takeError();
    }
    if (auto err =
            importModuleImpl(destModule, std::move(*mod), importInfo, true)) {
      return err;
    }
  }
  return llvm::Error::success();
}

} // end namespace whack::codegen

#endif // WHACK_MODULE_HPP
