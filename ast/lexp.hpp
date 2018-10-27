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
#ifndef WHACK_LEXP_HPP
#define WHACK_LEXP_HPP

#pragma once

#include "term.hpp"
#include "type.hpp"

namespace whack::ast {

class Lexp final : public Expression {
public:
  explicit Lexp(const mpc_ast_t* const ast) : state_{ast->state} {
    if (getInnermostAstTag(ast) == "lexp") {
      initial_ = std::make_unique<Term>(ast->children[0]);
      for (auto i = 1; i < ast->children_num; i += 2) {
        others_.emplace_back(
            std::pair{ast->children[i]->contents,
                      std::make_unique<Term>(ast->children[i + 1])});
      }
    } else {
      initial_ = std::make_unique<Term>(ast);
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto left = initial_->codegen(builder);
    if (!left) {
      return left.takeError();
    }
    auto lhs = *left;
    if (others_.empty()) {
      return lhs;
    }
    const auto module = builder.GetInsertBlock()->getModule();
    for (const auto& [op, value] : others_) {
      auto right = value->codegen(builder);
      if (!right) {
        return right.takeError();
      }
      const auto rhs = *right;
      if (lhs->getType() != rhs->getType()) {
        return error("type mismatch: cannot apply op at line {}",
                     state_.row + 1);
      }
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
          return error("invalid operator `{}` at line {}", op, state_.row + 1);
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
  using term_t = std::unique_ptr<Term>;
  term_t initial_;
  std::vector<std::pair<std::string, term_t>> others_;
};

} // end namespace whack::ast

#endif // WHACK_LEXP_HPP
