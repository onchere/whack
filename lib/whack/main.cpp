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
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/ManagedStatic.h>
#include <whack/codegen/module.hpp>

// Some command-line args
std::string InputFilename;
std::string GrammarFilename;
std::string OutputObjectFilename;
std::string OutputExecutableFilename;
bool EmitLLVM;
OptLevel OptimizationLevel;
SizeOptLevel SizeOptimizationLevel;
std::vector<std::string> ModuleSearchPaths; // @todo -I

using namespace llvm;

// @todo LLVM adds its opt CL options.

static cl::opt<std::string, true> inputFile(cl::Positional,
                                            cl::desc("<input file>"),
                                            cl::Required,
                                            cl::location(InputFilename));

static cl::opt<std::string, true>
    grammarFile("g", cl::desc("Specify the grammar filename"),
                cl::value_desc("filename"), cl::Required,
                cl::location(GrammarFilename));

static cl::opt<std::string, true>
    outputFile("o", cl::desc("Specify the output object filename"),
               cl::value_desc("filename"), cl::location(OutputObjectFilename),
               cl::init(""));

static cl::opt<std::string, true>
    exeFile("e", cl::desc("Specify the output executable filename"),
            cl::value_desc("filename"), cl::location(OutputExecutableFilename),
            cl::init(""));

static cl::opt<bool, true> emitLLVM("emit-llvm",
                                    cl::desc("Whether to emit LLVM IR"),
                                    cl::location(EmitLLVM), cl::init(false));

static cl::opt<OptLevel, true>
    optLevel(cl::desc("Choose an optimization level:"),
             cl::location(OptimizationLevel), cl::init(d),
             cl::values(clEnumVal(d, "No optimizations, enable debugging"),
                        clEnumVal(O1, "Enable trivial optimizations"),
                        clEnumVal(O2, "Enable default optimizations"),
                        clEnumVal(O3, "Enable aggresive optimizations")));

static cl::opt<SizeOptLevel, true>
    sizeOptLevel(cl::desc("Choose a size optimization level:"),
                 cl::location(SizeOptimizationLevel), cl::init(O0),
                 cl::values(clEnumVal(O0, "No size optimizations"),
                            clEnumVal(Os, "Optimize for size"),
                            clEnumVal(Oz, "Optimize for minimum size")));

int main(int argc, char** argv) {
  constexpr static auto Banner = "The Whack Compiler (pre-alpha)";
  cl::ParseCommandLineOptions(argc, argv, Banner);
  llvm::llvm_shutdown_obj{};
  if (auto err = whack::codegen::Module{}.compile()) {
    report_fatal_error(std::move(err));
  }
  return 0;
}
