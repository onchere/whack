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
#ifndef WHACK_MODULEUSE_HPP
#define WHACK_MODULEUSE_HPP

#pragma once

#include "identifier.hpp"

namespace whack::ast {

class ModuleUse final : public AST {
public:
  explicit ModuleUse(const mpc_ast_t* const ast)
      : identifier_{getIdentifier(ast->children[1])} {
    if (std::string_view(ast->children[2]->contents) == "hiding") {
      hiding_ = getIdentList(ast->children[3]);
    }
    const auto num = ast->children_num;
    if (std::string_view(ast->children[num - 2]->contents) == "qualified") {
      qualified_ = true;
    } else if (std::string_view(ast->children[num - 3]->contents) == "as") {
      as_ = ast->children[num - 2]->contents;
    }
  }

  inline const auto& identifier() const noexcept { return identifier_; }
  inline const auto& hiding() const noexcept { return hiding_; }
  inline const bool qualified() const noexcept { return qualified_; }

private:
  const identifier_t identifier_;
  ident_list_t hiding_;
  std::optional<std::string_view> as_;
  bool qualified_{false};
};

} // end namespace whack::ast

#endif // WHACK_MODULEUSE_HPP
