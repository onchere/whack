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
#ifndef WHACK_STRING_HPP
#define WHACK_STRING_HPP

#pragma once

namespace whack::codegen::expressions::factors {

// @todo
class String final : public Factor {
public:
  explicit String(const mpc_ast_t* const ast)
      : Factor(kString), string_{ast->contents} {
    string_ = string_.drop_front();
    string_ = string_.drop_back();
  }

  inline llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>& builder) const final {
    return builder.CreateGlobalStringPtr(string_);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kString;
  }

private:
  llvm::StringRef string_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_STRING_HPP
