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
#ifndef WHACK_ELEMENT_HPP
#define WHACK_ELEMENT_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Element final : public Factor {
public:
  explicit Element(const mpc_ast_t* const ast)
      : Factor(kElement), state_{ast->state},
        container_{getFactor(ast->children[0]->children[0])},
        index_{getExpressionValue(ast->children[1])} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto container = container_->codegen(builder);
    if (!container) {
      return container.takeError();
    }
    auto cont = getLoadedValue(builder, *container);
    const auto typeError = [&] {
      return error("expected an aggregate type to extract element at line {}",
                   state_.row + 1);
    };
    auto contType = cont->getType();
    if (llvm::isa<llvm::LoadInst>(cont)) {
      cont = llvm::cast<llvm::LoadInst>(cont)->getPointerOperand();
      contType = cont->getType()->getPointerElementType();
    }
    if (!(contType->isAggregateType() || contType->isPointerTy())) {
      return typeError();
    }
    auto index = index_->codegen(builder);
    if (!index) {
      return index.takeError();
    }
    const auto idx = getLoadedValue(builder, *index);
    llvm::Value* elt;
    if (contType->isArrayTy()) {
      elt = builder.CreateInBoundsGEP(cont, {Integral::get(0), idx});
    } else {
      elt = builder.CreateGEP(cont, idx);
    }
    const auto ret = builder.CreateAlloca(elt->getType(), 0, nullptr, "");
    builder.CreateStore(elt, ret);
    setIsDereferenceable(builder.getContext(), ret);
    return ret;
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kElement;
  }

private:
  const mpc_state_t state_;
  const std::unique_ptr<Factor> container_;
  const expr_t index_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_ELEMENT_HPP
