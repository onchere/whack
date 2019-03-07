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
#ifndef WHACK_ASSIGN_HPP
#define WHACK_ASSIGN_HPP

#pragma once

namespace whack::codegen::stmts {

class Assign final : public Stmt {
public:
  explicit Assign(const mpc_ast_t* const ast)
      : Stmt(kAssign), state_{ast->state} {
    auto idx = 0;
    using namespace expressions;
    for (; idx < ast->children_num; ++idx) {
      const auto ref = ast->children[idx];
      const std::string_view view{ref->contents};
      if (view == "=") {
        break;
      }
      if (view == ",") {
        continue;
      }
      variables_.push_back(factors::getFactor(ref));
    }
    exprList_ = getExprList(ast->children[++idx]);
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (exprList_.size() > variables_.size()) {
      return error("invalid number of values to assign "
                   "to at line {}",
                   state_.row + 1);
    } else if (variables_.size() > exprList_.size()) {
      if (exprList_.size() != 1) {
        return error("structured binding expects a single "
                     "expression on RHS at line {}",
                     state_.row + 1);
      }
    }

    const auto store =
        [&](llvm::Value* const value,
            const expressions::factors::Factor* const var) -> llvm::Error {
      auto v = var->codegen(builder);
      if (!v) {
        return v.takeError();
      }
      auto variable = *v;
      // we discard the store
      if (variable->getName() == "_") {
        return llvm::Error::success();
      }
      if (hasMetadata(variable, llvm::LLVMContext::MD_dereferenceable)) {
        variable = align(builder.CreateLoad(variable));
      }
      const auto varType = variable->getType()->getPointerElementType();
      if (value->getType() != varType) {
        return error("type mismatch: cannot assign at line {}", state_.row + 1);
      }
      align(builder.CreateStore(value, variable));
      return llvm::Error::success();
    };

    if (variables_.size() > exprList_.size()) {
      auto v = exprList_[0]->codegen(builder);
      if (!v) {
        return v.takeError();
      }
      const auto value = *v;
      const auto type = value->getType();
      if (!llvm::isa<llvm::CompositeType>(type)) {
        return error("invalid type for structured bindings "
                     "at line {}",
                     state_.row + 1);
      }
      const auto numExpected =
          type->isStructTy()
              ? type->getStructNumElements()
              : llvm::cast<llvm::SequentialType>(type)->getNumElements();
      if (numExpected != variables_.size()) {
        return error("invalid number of variables to bind"
                     " expression to at line {} "
                     "(expected {}, got {})",
                     state_.row + 1, numExpected, variables_.size());
      }
      for (size_t i = 0; i < variables_.size(); ++i) {
        llvm::Value* val;
        if (type->isStructTy()) {
          val = builder.CreateExtractValue(value, i);
        } else { // is array/vector
          val = builder.CreateExtractElement(value, i);
        }
        if (auto err = store(val, variables_[i].get())) {
          return err;
        }
      }
    } else {
      for (size_t i = 0; i < exprList_.size(); ++i) {
        auto val = exprList_[i]->codegen(builder);
        if (!val) {
          return val.takeError();
        }
        auto v = expressions::getLoadedValue(builder, *val);
        if (!v) {
          return v.takeError();
        }
        if (auto err = store(*v, variables_[i].get())) {
          return err;
        }
      }
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kAssign;
  }

private:
  const mpc_state_t state_;
  small_vector<std::unique_ptr<expressions::factors::Factor>> variables_;
  small_vector<expr_t> exprList_;
};

} // end namespace whack::codegen::stmts

#endif // WHACK_ASSIGN_HPP
