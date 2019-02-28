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
#ifndef WHACK_ELEMENT_FACTOR_HPP
#define WHACK_ELEMENT_FACTOR_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class Element {
public:
  static llvm::Expected<llvm::Value*>
  get(llvm::IRBuilder<>& builder, llvm::Value* cont, llvm::Value* const idx) {
    const auto typeError = [&] {
      return error("expected an aggregate type to extract element");
    };
    auto contType = cont->getType();
    if (llvm::isa<llvm::LoadInst>(cont)) {
      const auto operand =
          llvm::cast<llvm::LoadInst>(cont)->getPointerOperand();
      if (llvm::isa<llvm::AllocaInst>(operand)) {
        cont = operand;
        contType = cont->getType()->getPointerElementType();
      }
    }
    if (!(contType->isAggregateType() || contType->isPointerTy())) {
      return typeError();
    }
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
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_ELEMENT_FACTOR_HPP
