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
#ifndef WHACK_POSTOP_HPP
#define WHACK_POSTOP_HPP

#pragma once

#include "ast.hpp"

namespace whack::ast {

class PostOp final : public Factor {
public:
  explicit PostOp(const mpc_ast_t* const ast)
      : state_{ast->state}, val_{getFactor(ast->children[1])},
        op_{ast->children[0]->contents} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto v = val_->codegen(builder);
    if (!v) {
      return v.takeError();
    }
    const auto val = *v;
    const auto value = builder.CreateLoad(val);
    const auto type = value->getType();
    if (type->isIntegerTy() || type->isFloatingPointTy()) {
      auto op = op_;
      op.pop_back();
      if (OpsTable.find(op) != OpsTable.end()) {
        const auto incr = type->isIntegerTy()
                              ? llvm::ConstantInt::get(type, 1)
                              : llvm::ConstantFP::get(type, 1.0);
        const auto modified = llvm::unwrap(OpsTable[op](
            llvm::wrap(&builder), llvm::wrap(value), llvm::wrap(incr), ""));
        builder.CreateStore(modified, val);
        return val;
      }
      return error("operator {} not allowed for type "
                   "at line {}",
                   op_, state_.row + 1);
    } else if (const auto [structType, isStruct] = Type::isStructKind(type);
               isStruct) {
      const auto structName = structType->getStructName().str();
      const auto module = builder.GetInsertBlock()->getModule();
      if (const auto func = module->getFunction(
              format("struct::{}::operator{}", structName, op_))) {
        if (func->getReturnType() == BasicTypes["void"]) {
          builder.CreateCall(func, val);
          return val;
        } else {
          const auto modified = builder.CreateCall(func, val);
          builder.CreateStore(modified, val);
          return val;
        }
      } else {
        return error("cannot find operator{} for struct `{}` "
                     "at line {}",
                     op_, structName, state_.row + 1);
      }
    }
    return error("invalid type for operator {} at line {}", op_,
                 state_.row + 1);
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<Factor> val_;
  const std::string op_;
};

class PostOpStmt final : public Stmt {
public:
  explicit PostOpStmt(const mpc_ast_t* const ast) noexcept
      : Stmt(kPostOp), impl_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (auto val = impl_.codegen(builder); !val) {
      return val.takeError();
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kPostOp;
  }

private:
  const PostOp impl_;
};

} // end namespace whack::ast

#endif // WHACK_POSTOP_HPP
