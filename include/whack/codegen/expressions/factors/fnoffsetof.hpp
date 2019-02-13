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
#ifndef WHACK_FNOFFSETOF_HPP
#define WHACK_FNOFFSETOF_HPP

#pragma once

namespace whack::codegen::expressions::factors {

class FnOffsetOf final : public Factor {
public:
  explicit FnOffsetOf(const mpc_ast_t* const ast)
      : Factor(kFnOffsetOf), state_{ast->state}, type_{ast->children[2]},
        memberName_{ast->children[4]->contents} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto type = type_.codegen(builder);
    if (!type) {
      return type.takeError();
    }
    if (!(*type)->isStructTy()) {
      return error("expected struct type for offsetof function "
                   "at line {}",
                   state_.row + 1);
    }
    const auto structName = (*type)->getStructName();
    const auto index = StructMember::getIndex(
        *builder.GetInsertBlock()->getModule(), structName, memberName_);
    if (!index) {
      return error("member `{}` does not exist for struct `{}` "
                   "at line {}",
                   memberName_.data(), structName.data(), state_.row + 1);
    }
    return llvm::ConstantExpr::getOffsetOf(llvm::cast<llvm::StructType>(*type),
                                           index.value());
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kFnOffsetOf;
  }

private:
  const mpc_state_t state_;
  const types::Type type_;
  const llvm::StringRef memberName_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_FNOFFSETOF_HPP
