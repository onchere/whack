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
#ifndef WHACK_POSTOP_HPP
#define WHACK_POSTOP_HPP

#pragma once

namespace whack::codegen::expressions {

namespace operators {
static std::optional<llvm::Value*> applyStructOperator(llvm::IRBuilder<>&,
                                                       llvm::Value* const,
                                                       const llvm::StringRef,
                                                       llvm::Value* const);

static llvm::Expected<llvm::Value*> getAdditive(llvm::IRBuilder<>&,
                                                llvm::Value* const,
                                                const llvm::StringRef,
                                                llvm::Value* const);
} // end namespace operators

namespace factors {

class PostOp final : public Factor {
public:
  explicit PostOp(const mpc_ast_t* const ast)
      : Factor(kPostOp), state_{ast->state}, val_{getFactor(ast->children[0])},
        op_{ast->children[1]->contents} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto v = val_->codegen(builder);
    if (!v) {
      return v.takeError();
    }
    const auto val = *v;
    const auto value = builder.CreateLoad(val);
    const auto type = value->getType();
    if (type->isIntegerTy() || type->isFloatingPointTy()) {
      const auto incr = type->isIntegerTy() ? llvm::ConstantInt::get(type, 1)
                                            : llvm::ConstantFP::get(type, 1.0);
      using namespace expressions::operators;
      auto apply =
          operators::getAdditive(builder, value, op_.drop_front(), incr);
      if (apply) {
        builder.CreateStore(*apply, val);
        return value;
      }
      return apply.takeError();
    }
    if (type->isStructTy()) {
      static const auto postOpTag =
          llvm::ConstantInt::get(BasicTypes["int"], 1);
      if (auto apply =
              operators::applyStructOperator(builder, val, op_, postOpTag)) {
        if ((*apply)->getType()->isVoidTy()) {
          return *apply;
        }
        return llvm::cast<llvm::Value>(value);
      }
      return error("cannot find operator{} for struct `{}` "
                   "at line {}",
                   op_.data(), type->getStructName().data(), state_.row + 1);
    }
    return error("invalid type for operator{} at line {}", op_.data(),
                 state_.row + 1);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kPostOp;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<Factor> val_;
  const llvm::StringRef op_;
};

class PostOpStmt final : public stmts::Stmt {
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

} // end namespace factors
} // end namespace whack::codegen::expressions

#endif // WHACK_POSTOP_HPP
