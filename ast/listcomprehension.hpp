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
#ifndef WHACK_LISTCOMPREHENSION_HPP
#define WHACK_LISTCOMPREHENSION_HPP

#pragma once

#include "comparison.hpp"
#include "forinexpr.hpp"
#include "opeq.hpp"

namespace whack::ast {

class ListComprehension final : public Factor {
public:
  explicit ListComprehension(const mpc_ast_t* const ast)
      : identList_{getIdentList(ast->children[1])} {
    for (auto i = 2; i < ast->children_num - 1; i += 2) {
      expr_.emplace_back(ForInExpr{ast->children[i]});
    }
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto& ctx = builder.getContext();
    const auto func = builder.GetInsertBlock()->getParent();
    for (const auto& expr : expr_) {
      const auto& range = expr.range();
      auto beg = range.begin(builder);
      if (!beg) {
        return beg.takeError();
      }
      const auto begin = *beg;
      const auto type = begin->getType();
      // we only loop over unnamed constants now...
      if (type->isPointerTy()) {
        if (type->isStructTy()) {
          // @todo : If next/end are provided, compile error
          if (Type::isVariableLengthArray(type)) {
            // @todo: get length, loop
          } else {
            // @todo: check for .begin() && .end()
          }
        }
        llvm_unreachable("not implemented");
      } else {
        // @todo: If no end require that we must be yielding if in a function,
        //  (otherwise yield on access for a variable??)
        auto e = range.end(builder);
        if (!e) {
          return e.takeError();
        }
        const auto end = *e;
        assert(end && "We require an end for now...");
        auto n = range.next(builder);
        if (!n) {
          return n.takeError();
        }

        const auto incr = range.hasNext() ? *n : Integral::get(1, type);
        const auto ret = builder.CreateAlloca(
            ArrayType::getVarLenType(ctx, type), 0, nullptr, "");
        const auto retType = ret->getType()->getPointerElementType();
        const auto sizePtr = builder.CreateStructGEP(retType, ret, 0);
        builder.CreateStore(Integral::get(0), sizePtr);

        const auto block = llvm::BasicBlock::Create(ctx, "block", func);
        const auto cont = llvm::BasicBlock::Create(ctx, "cont", func);
        const auto cmp = INTCMP[range.endInclusive() ? "<=" : "<"];
        const auto current = builder.CreateAlloca(type, 0, nullptr, "");

        builder.CreateStore(begin, current);
        builder.CreateCondBr(
            builder.CreateICmp(cmp, builder.CreateLoad(current), end), block,
            cont);
        builder.SetInsertPoint(block);

        const auto step = builder.CreateAdd(builder.CreateLoad(current), incr);
        builder.CreateStore(step, current);
        const auto arrPtr = builder.CreateStructGEP(retType, ret, 1);
        const auto ptr = builder.CreateInBoundsGEP(
            arrPtr, {Integral::get(0), builder.CreateLoad(sizePtr)});
        builder.CreateStore(builder.CreateLoad(current), ptr);
        const auto size =
            builder.CreateAdd(builder.CreateLoad(sizePtr), Integral::get(1));
        builder.CreateStore(size, sizePtr);

        builder.CreateCondBr(
            builder.CreateICmp(cmp, builder.CreateLoad(current), end), block,
            cont);
        builder.SetInsertPoint(cont);
        return builder.CreateLoad(ret);
      }
    }
    return error("list comprehension failed"); // @todo
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder,
                                       llvm::Type* const type,
                                       const mpc_state_t state) const {
    auto value = this->codegen(builder);
    if (value) {
      if ((*value)->getType() == type) {
        return value;
      }
      return error("invalid type provided at line {}", state.row + 1);
    }
    return value.takeError();
  }

private:
  ident_list_t identList_;
  small_vector<ForInExpr> expr_;
};

} // end namespace whack::ast

#endif // WHACK_LISTCOMPREHENSION_HPP
