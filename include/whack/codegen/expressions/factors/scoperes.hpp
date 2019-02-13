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
#ifndef WHACK_SCOPERES_HPP
#define WHACK_SCOPERES_HPP

#pragma once

#include <llvm/Support/raw_ostream.h>

namespace whack::codegen::expressions::factors {

class ScopeRes final : public Factor {
public:
  explicit ScopeRes(const mpc_ast_t* const ast)
      : Factor(kScopeRes), state_{ast->state} {
    llvm::raw_string_ostream os{name_};
    for (auto i = 0; i < ast->children_num; ++i) {
      const auto str = ast->children[i]->contents;
      os << str;
      if (i % 2 == 0) {
        parts_.push_back(str);
      }
    }
    os.flush();
  }

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    const auto module = builder.GetInsertBlock()->getModule();
    if (const auto GV = module->getNamedGlobal(name_)) {
      assert(GV->hasInitializer());
      return llvm::cast<llvm::Value>(GV->getInitializer());
    }
    for (auto& func : module->functions()) {
      if (func.getName() == name_) {
        return &func;
      }
    }
    return error("could not find symbol with the name "
                 "`{}` at line {}",
                 name_, state_.row + 1);
  }

  inline const auto& name() const { return name_; }

  inline const auto& parts() const { return parts_; }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kScopeRes;
  }

private:
  const mpc_state_t state_;
  std::string name_;
  small_vector<llvm::StringRef> parts_;
};

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_SCOPERES_HPP
