**whack**
=========
- A simply-designed compiled programming language.
- Can call C functions without ceremony.
- Suitable for learning some compiler construction basics.
- Also suitable as a base for further development.
- Also suitable for use as-is.
- ***Whack currently lacks a comprehensively designed type system.***
- **important** Progressively designed/implemented. Some constructs may seem whack.
- No tests (pull requests welcome (follow clang's test style)).
- Currently supporting Windows (I'm on Windows).
- To view current progress, run `whack.exe` in snapshot folder.

*Progress*
==========
Pre-Alpha.
This repository is a dump of what is currently on my Sublime Text.

*Non-Goals*
===========
- Industrial-strength compiler for our Stage 1/State 2 compiler.

*TODO*
=======
- [ ] Stage 1 compiler: Compile *whack* code using using C++ code): (*Current progress*).
- [ ] Stage 2 compiler: Use our Stage 1 compiler to compile the Whack compiler in *whack*.
- [ ] Stage 3 compiler: Use our State 2 compiler to make a self-hosted Whack compiler.
- [ ] Standard Library.

*Requirements*
==============
- Facebook.Folly (Release folly-2018.08.20.00).
- spdlog (Release spdlog-1.1.0).
- `gcc` must be available on your PATH (MinGW GCC is available at [Nuwen.net](http://nuwen.net)).
- LLVM (To be provided as a DLL in snapshot folder). For now, download/compile LLVM 6.0.1 DLL.
