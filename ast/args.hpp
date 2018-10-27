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
#ifndef WHACK_ARGS_HPP
#define WHACK_ARGS_HPP

#pragma once

#include "ast.hpp"
#include "type.hpp"

namespace whack::ast {

class Arg {
public:
  explicit Arg(Type&& type, llvm::StringRef name, const bool variadic = false,
               const bool mut = false)
      : type{std::forward<Type>(type)}, name{name}, variadic{variadic},
        mut{mut} {}

  const Type type;
  llvm::StringRef name;
  const bool variadic;
  const bool mut;
};

class Args final : public AST {
public:
  explicit Args(const mpc_ast_t* const ast) {
    const auto tag = getInnermostAstTag(ast);
    if (tag == "variadicarg") {
      args_.emplace_back(Arg{Type{ast->children[0]->children[0]},
                             ast->children[1]->contents, true});
    } else if (tag == "typeident") {
      args_.emplace_back(
          Arg{Type{ast->children[0]}, ast->children[1]->contents});
    } else {
      for (auto i = 0; i < ast->children_num; i += 2) {
        const auto ref = ast->children[i];
        if (getInnermostAstTag(ref) == "typeident") {
          args_.emplace_back(
              Arg{Type{ref->children[0]}, ref->children[1]->contents});
        } else { // variadicarg
          args_.emplace_back(Arg{Type{ref->children[0]->children[0]},
                                 ref->children[1]->contents, true});
        }
      }
    }
  }

  inline const auto& arg(const std::size_t index) const {
    return args_.at(index);
  }

  inline const auto& args() const noexcept { return args_; }

  const auto types(const llvm::Module* const module) const {
    small_vector<llvm::Type*> ret;
    for (const auto& arg : args_) {
      auto type = arg.type.codegen(module);
      if (Type::getUnderlyingType(type)->isFunctionTy()) {
        type = type->getPointerTo(0);
      }
      ret.push_back(type);
    }
    return ret;
  }

  const auto names() const {
    small_vector<llvm::StringRef> ret;
    for (const auto& arg : args_) {
      ret.push_back(arg.name);
    }
    return ret;
  }

  inline const auto size() const { return args_.size(); }

  inline const bool variadic() const {
    return !args_.empty() && args_.back().variadic;
  }

private:
  std::vector<Arg> args_;
};

} // end namespace whack::ast

#endif // WHACK_ARGS_HPP
