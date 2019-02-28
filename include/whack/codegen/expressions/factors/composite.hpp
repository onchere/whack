/**
 * Copyright 2019-present Onchere Bironga
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
#ifndef WHACK_COMPOSITE_HPP
#define WHACK_COMPOSITE_HPP

#pragma once

#include "element.hpp"
#include "expandop.hpp"
#include "funccall.hpp"
#include "postop.hpp"
#include "structmember.hpp"

namespace whack::codegen::expressions::factors {

class CompositeFactor : public Factor {
public:
  explicit CompositeFactor(const mpc_ast_t* const ast)
      : Factor(kComposite), base_{getFactor(ast->children[0])},
        composite_{ast->children[1]} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    using ret_t = llvm::Expected<llvm::Value*>;
    std::function<ret_t(llvm::Value* const, const mpc_ast_t* const)>
        aggregateMember;
    aggregateMember = [&builder, &aggregateMember](
                          llvm::Value* const base,
                          const mpc_ast_t* const composite) -> ret_t {
      if (!composite->children_num) {
        const std::string_view hint{composite->contents};
        if (hint == "..") {
          // @todo Range iterator
          return error("range iteration not implemented");
        }
        if (hint == "++" || hint == "--") {
          auto var = getLoadedValue(builder, base, false);
          if (!var) {
            return var.takeError();
          }
          return PostOp::get(builder, *var, composite->contents);
        }
        if (hint == "&") {
          return Reference::get(builder, base);
        }
        // has to be an expand op
        return ExpandOp::get(builder, base);
      }
      const std::string_view hint{composite->children[0]->contents};
      if (hint == "..") {
        // @todo Range
        return error("ranges not implemented");
      }
      if (hint == "(") {
        auto func = base;
        if (llvm::isa<llvm::AllocaInst>(base)) {
          func = builder.CreateLoad(base);
        }
        small_vector<llvm::Value*> funcs = {func};
        small_vector<llvm::Value*> arguments;
        const mpc_ast_t* next = nullptr;
        if (composite->children_num > 2 &&
            getOutermostAstTag(composite->children[1]) == "exprlist") {
          const auto exprList = getExprList(composite->children[1]);
          auto args = getExprValues(builder, exprList, true);
          if (!args) {
            return args.takeError();
          }
          arguments = std::move(*args);
          if (composite->children_num > 3) {
            next = composite->children[3];
          }
        } else {
          if (composite->children_num > 2) {
            next = composite->children[2];
          }
        }
        auto call = FuncCall::get(builder, funcs, arguments);
        if (!call) {
          return call.takeError();
        }
        if (next != nullptr) {
          return aggregateMember(*call, next);
        }
        return *call;
      }
      if (hint == ".") {
        auto mem = StructMember::get(builder, base, composite->children[1]);
        if (!mem) {
          return mem.takeError();
        }
        if (composite->children_num > 2) {
          return aggregateMember(*mem, composite->children[2]);
        }
        return *mem;
      }
      // has to be element access
      auto idx = getExpressionValue(composite->children[1])->codegen(builder);
      if (!idx) {
        return idx.takeError();
      }
      auto index = getLoadedValue(builder, *idx, false);
      if (!index) {
        return index.takeError();
      }
      auto cont = getLoadedValue(builder, base, false);
      if (!cont) {
        return cont.takeError();
      }
      auto elt = Element::get(builder, *cont, *index);
      if (!elt) {
        return elt.takeError();
      }
      if (composite->children_num > 3) {
        return aggregateMember(*elt, composite->children[3]);
      }
      return *elt;
    };
    auto base = base_->codegen(builder);
    if (!base) {
      return base.takeError();
    }
    return aggregateMember(*base, composite_);
  }

  inline static constexpr bool classof(const Factor* const factor) {
    return factor->getKind() == kComposite;
  }

private:
  const std::unique_ptr<Factor> base_;
  const mpc_ast_t* const composite_;
};

} // namespace whack::codegen::expressions::factors

#endif // WHACK_COMPOSITE_HPP
