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
#ifndef WHACK_EXPANDOP_HPP
#define WHACK_EXPANDOP_HPP

#pragma once

namespace whack::codegen::expressions::factors {

// Implements parameter list expansion e.g. fun(params...)
class ExpandOp {
public:
  static llvm::Expected<llvm::Value*> get(llvm::IRBuilder<>& builder,
                                          llvm::Value* const value) {
    return error("ExpandOp not implemented (relies on coroutines)!");
  }
};

} // namespace whack::codegen::expressions::factors

#endif // WHACK_EXPANDOP_HPP
