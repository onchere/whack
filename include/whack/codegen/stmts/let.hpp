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
#ifndef WHACK_LET_HPP
#define WHACK_LET_HPP

#pragma once

namespace whack::codegen::stmts {

class Let final : public Stmt {
public:
  explicit Let(const mpc_ast_t* const ast)
      : Stmt(kLet), state_{ast->state},
        varsAreMutable_{std::string_view(ast->children[1]->contents) == "mut"},
        identList_{getIdentList(ast->children[varsAreMutable_ ? 2 : 1])},
        exprList_{
            expressions::getExprList(ast->children[varsAreMutable_ ? 4 : 3])} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    using namespace expressions::factors;
    if (identList_.size() > 1 && exprList_.size() == 1) {
      auto expr = exprList_[0]->codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      if (!(*expr)->getType()->isAggregateType()) {
        return error("invalid number of values to assign from "
                     "at line {} (expected an aggregate type on RHS)",
                     state_.row + 1);
      }
      for (size_t i = 0; i < identList_.size(); ++i) {
        const auto& name = identList_[i];
        if (name == "_") { // we don't store the value
          continue;
        }
        if (auto err = Ident::isUnique(builder, name)) {
          return err;
        }
        const auto value = builder.CreateExtractValue(*expr, i, "");
        const auto alloc =
            builder.CreateAlloca(value->getType(), 0, nullptr, name);
        builder.CreateStore(value, alloc);
      }
    } else if (exprList_.size() == identList_.size()) {
      for (size_t i = 0; i < identList_.size(); ++i) {
        auto val = exprList_[i]->codegen(builder);
        if (!val) {
          return val.takeError();
        }
        const auto& name = identList_[i];
        if (name == "_") { // we don't store the value
          continue;
        }
        if (auto err = Ident::isUnique(builder, name)) {
          return err;
        }
        const auto value = *val;
        constexpr static auto refMD = llvm::LLVMContext::MD_dereferenceable;
        if (llvm::isa<llvm::AllocaInst>(value) && hasMetadata(value, refMD)) {
          value->setName(name);
        } else {
          auto v = expressions::getLoadedValue(builder, *val);
          if (!v) {
            return v.takeError();
          }
          const auto value = *v;
          const auto alloc =
              builder.CreateAlloca(value->getType(), 0, nullptr, name);
          builder.CreateStore(value, alloc);
          if (hasMetadata(value, refMD)) {
            setIsDereferenceable(builder.getContext(), alloc);
          }
        }
      }
    } else {
      return error("invalid number of variables to assign to "
                   "at line {}",
                   state_.row + 1);
    }
    return llvm::Error::success();
  }

  inline const auto& variables() const { return identList_; }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kLet;
  }

private:
  const mpc_state_t state_;
  const bool varsAreMutable_;
  const ident_list_t identList_;
  const small_vector<expr_t> exprList_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_LETEXPR_HPP
