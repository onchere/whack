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
#ifndef WHACK_TERM_HPP
#define WHACK_TERM_HPP

#pragma once

#include "ast.hpp"
#include "type.hpp"

namespace whack::ast {

class Term final : public AST {
public:
  explicit Term(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "term") {
      initial_ = getFactor(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_.emplace_back(std::pair{ast->children[i]->contents,
                                       getFactor(ast->children[i + 1])});
      }
    } else {
      initial_ = getFactor(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) {
    auto init = initial_->codegen(builder);
    if (!init) {
      return init.takeError();
    }
    auto lhs = *init;
    if (others_.empty()) {
      if (const auto [_, isStruct] = Type::isStructKind(lhs->getType());
          isStruct) {
        return lhs;
      }
      if (llvm::isa<llvm::AllocaInst>(lhs)) {
        return builder.CreateLoad(lhs);
      }
      return lhs;
    }

    if (llvm::isa<llvm::AllocaInst>(lhs)) {
      lhs = builder.CreateLoad(lhs);
    }

    const auto module = builder.GetInsertBlock()->getModule();
    for (const auto& [op, factor] : others_) {
      auto val = factor->codegen(builder);
      if (!val) {
        return val.takeError();
      }
      auto value = *val;
      const auto rhs = llvm::isa<llvm::AllocaInst>(value)
                           ? builder.CreateLoad(value)
                           : value;
      const auto [structType, isStruct] = Type::isStructKind(lhs->getType());
      if (isStruct) {
        const auto structName = structType->getStructName().str();
        if (const auto func = module->getFunction(
                format("struct::{}::operator{}", structName, op))) {
          lhs = builder.CreateCall(func, {lhs, rhs});
        } else {
          return error("cannot find operator{} for struct `{}` "
                       "at line {}",
                       "+", structName, state_.row + 1);
        }
      } else {
        auto ops = op;
        if (lhs->getType()->isFloatingPointTy() &&
            (op == "+" || op == "-" || op == "*" || op == "/")) {
          ops += "f";
        }
        if (OpsTable.find(ops) == OpsTable.end()) {
          return error("invalid operator `{}` at line {}", ops, state_.row + 1);
        }
        lhs = reinterpret_cast<llvm::Value*>(
            OpsTable[ops](reinterpret_cast<LLVMBuilderRef>(&builder),
                          reinterpret_cast<LLVMValueRef>(lhs),
                          reinterpret_cast<LLVMValueRef>(rhs), ""));
      }
    }
    return lhs;
  }

private:
  const mpc_state_t state_;
  using factor_t = std::unique_ptr<Factor>;
  factor_t initial_;
  std::vector<std::pair<std::string, factor_t>> others_;
};

} // end namespace whack::ast

#endif // WHACK_TERM_HPP
