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
#ifndef WHACK_FACTOR_HPP
#define WHACK_FACTOR_HPP

#pragma once

#include "../../fwd.hpp"
#include "binary.hpp"
#include "boolean.hpp"
#include "character.hpp"
#include "closure.hpp"
#include "composite.hpp"
#include "deref.hpp"
#include "expansion.hpp"
#include "floatingpt.hpp"
#include "fnalignof.hpp"
#include "fncast.hpp"
#include "fnoffsetof.hpp"
#include "fnsizeof.hpp"
#include "funccall.hpp"
#include "hexadecimal.hpp"
#include "ident.hpp"
#include "initlist.hpp"
#include "integral.hpp"
#include "matchexpr.hpp"
#include "memberinitlist.hpp"
#include "neg.hpp"
#include "newexpr.hpp"
#include "not.hpp"
#include "nullptr.hpp"
#include "octal.hpp"
#include "overloadid.hpp"
#include "preop.hpp"
#include "reference.hpp"
#include "scoperes.hpp"
#include "string.hpp"
#include "value.hpp"

namespace whack::codegen::expressions::factors {

class ExpressionFactor final : public Factor {
public:
  explicit ExpressionFactor(const mpc_ast_t* const ast)
      : Factor(kExpression), factor_{getExpressionValue(ast)} {}

  inline llvm::Expected<llvm::Value*>
  codegen(llvm::IRBuilder<>& builder) const final {
    return factor_->codegen(builder);
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kExpression;
  }

private:
  const expr_t factor_;
};

static std::unique_ptr<Factor> getFactor(const mpc_ast_t* const ast) {
  const auto tag = getInnermostAstTag(ast);
  if (tag == "factor") {
    const std::string_view view{ast->children[0]->contents};
    if (view == "(") {
      return std::make_unique<ExpressionFactor>(ast->children[1]);
    }
    if (view == "!") {
      return std::make_unique<Not>(ast->children[1]);
    }
    if (view == "*") {
      return std::make_unique<Deref>(ast->children[1]);
    }
    if (view == "+") {
      return getFactor(ast->children[1]);
    }
    if (view == "-") {
      return std::make_unique<Neg>(ast->children[1]);
    }
    if (view == "~") {
      // @todo
    }
    if (view == "++" || view == "--") {
      return std::make_unique<PreOp>(ast);
    }
    return std::make_unique<CompositeFactor>(ast);
  }

#define OPT(RULE, CLASS)                                                       \
  if (tag == RULE) {                                                           \
    return std::make_unique<CLASS>(ast);                                       \
  }
  OPT("matchexpr", MatchExpr)
  OPT("closure", Closure)
  OPT("newexpr", NewExpr)
  OPT("sizeofval", FnSizeOf)
  OPT("alignofval", FnAlignOf)
  OPT("offsetofval", FnOffsetOf)
  OPT("cast", FnCast)
  OPT("value", Value)
  OPT("memberinitlist", MemberInitList)
  OPT("initlist", InitList)
  OPT("character", Character)
  OPT("floatingpt", FloatingPt)
  OPT("binary", Binary)
  OPT("octal", Octal)
  OPT("hexadecimal", HexaDecimal)
  OPT("integral", Integral)
  OPT("boolean", Boolean)
  OPT("scoperes", ScopeRes)
#undef OPT

  if (tag == "ident") {
    if (std::string_view(ast->contents) == "nullptr") {
      return std::make_unique<NullPtr>();
    }
    return std::make_unique<Ident>(ast);
  }

  if (tag == "string") {
    const std::string_view view{ast->contents};
    if (view == "true" || view == "false") {
      return std::make_unique<Boolean>(ast);
    }
    if (view == "...") {
      return std::make_unique<Expansion>();
    }
    return std::make_unique<String>(ast);
  }
  llvm_unreachable("invalid factor kind!");
}

} // end namespace whack::codegen::expressions::factors

#endif // WHACK_FACTOR_HPP
