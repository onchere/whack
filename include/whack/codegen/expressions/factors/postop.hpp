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

class PostOp {
public:
  static llvm::Expected<llvm::Value*>
  get(llvm::IRBuilder<>& builder, llvm::Value* val, const llvm::StringRef op) {
    llvm::Value* value;
    if (llvm::isa<llvm::LoadInst>(val)) {
      value = val;
      val = llvm::cast<llvm::LoadInst>(val)->getPointerOperand();
    } else {
      value = builder.CreateLoad(val);
    }
    const auto type = value->getType();
    if (type->isIntegerTy() || type->isFloatingPointTy()) {
      const auto incr = type->isIntegerTy() ? llvm::ConstantInt::get(type, 1)
                                            : llvm::ConstantFP::get(type, 1.0);
      using namespace expressions::operators;
      auto apply =
          operators::getAdditive(builder, value, op.drop_front(), incr);
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
              operators::applyStructOperator(builder, val, op, postOpTag)) {
        if ((*apply)->getType()->isVoidTy()) {
          return *apply;
        }
        return llvm::cast<llvm::Value>(value);
      }
      return error("cannot find operator{} for struct `{}`", op.data(),
                   type->getStructName().data());
    }
    return error("invalid type for operator{}", op.data());
  }
};

} // end namespace factors
} // end namespace whack::codegen::expressions

#endif // WHACK_POSTOP_HPP
