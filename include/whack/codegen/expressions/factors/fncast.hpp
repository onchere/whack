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
#ifndef WHACK_FNCAST_HPP
#define WHACK_FNCAST_HPP

#pragma once

#include "../../elements/interface.hpp"

namespace whack::codegen::expressions::factors {

class FnCast final : public Factor {
public:
  explicit FnCast(const mpc_ast_t* const ast)
      : Factor(kFnCast), state_{ast->state}, typeTo_{ast->children[2]},
        expr_{getExpressionValue(ast->children[5])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto e = expr_->codegen(builder);
    if (!e) {
      return e.takeError();
    }
    const auto expr = *e;
    const auto typeFrom = expr->getType();
    const auto module = builder.GetInsertBlock()->getModule();
    auto tp = typeTo_.codegen(builder);
    if (!tp) {
      return tp.takeError();
    }
    const auto typeTo = *tp;
    if (!typeTo) {
      return error("type does not exist in scope at line {}", state_.row + 1);
    }
    if (typeFrom->isIntegerTy()) {
      if (typeTo->isFloatingPointTy()) {
        return builder.CreateSIToFP(expr, typeTo);
      }
      return builder.CreateIntCast(expr, typeTo, false /*@todo IsSigned?*/);
    } else if (typeFrom->isFloatingPointTy()) {
      if (typeTo->isIntegerTy()) {
        return builder.CreateFPToSI(expr, typeTo);
      }
      return builder.CreateFPCast(expr, typeTo);
    } else if (typeFrom->isPointerTy()) {
      const auto [type, isStruct] = types::Type::isStructKind(typeFrom);
      if (isStruct) {
        const auto [to, toIsStruct] = types::Type::isStructKind(typeTo);
        // @todo Data Class
        if (toIsStruct && to->getStructName().startswith("interface::")) {
          auto impl = elements::Interface::cast(builder, typeTo, expr);
          if (!impl) {
            return impl.takeError();
          }
          return *impl;
        } else {
          const auto funcName =
              format("struct::{}::operator {}", type->getStructName().data(),
                     getTypeName(typeTo));
          if (auto caster = module->getFunction(funcName)) {
            // @todo Check arity?
            return builder.CreateCall(caster, expr);
          }
        }
      } else if (typeFrom->getPointerElementType() == BasicTypes["char"]) {
        if (typeTo->isIntegerTy() || typeTo->isFloatingPointTy()) {
          return error("parsing numbers from char* not implemented "
                       "at line {}",
                       state_.row + 1);
        }
        // assert(typeTo->isPointerTy());
        return builder.CreateBitOrPointerCast(expr, typeTo);
      }
    }
    return error("invalid cast at line {}", state_.row + 1);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kFnCast;
  }

private:
  const mpc_state_t state_;
  const types::Type typeTo_;
  std::unique_ptr<Expression> expr_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_FNCAST_HPP
