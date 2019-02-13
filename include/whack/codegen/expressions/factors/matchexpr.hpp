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
#ifndef WHACK_MATCHEXPR_HPP
#define WHACK_MATCHEXPR_HPP

#pragma once

namespace whack::codegen::expressions {

namespace operators {
static llvm::Expected<llvm::Value*> getEquality(llvm::IRBuilder<>&,
                                                llvm::Value* const,
                                                const llvm::StringRef,
                                                llvm::Value* const);
} // end namespace operators

namespace factors {

class MatchExpr final : public Factor {
public:
  explicit constexpr MatchExpr(const mpc_ast_t* const ast) noexcept
      : Factor(kMatchExpr), ast_{ast} {}

  llvm::Expected<llvm::Value*> codegen(llvm::IRBuilder<>& builder) const final {
    auto info = getMatchInfo(ast_, builder);
    if (!info) {
      return info.takeError();
    }
    const auto& matchInfo = *info;
    const auto equals =
        [&](const auto& options) -> llvm::Expected<llvm::Value*> {
      llvm::Value* ret = builder.getFalse();
      for (size_t i = 0; i < options.size(); ++i) {
        auto cmp = operators::getEquality(builder, matchInfo.Subject,
                                          "==", options[i]);
        if (!cmp) {
          return cmp.takeError();
        }
        ret = builder.CreateOr(ret, *cmp);
      }
      return ret;
    };

    const auto& last = matchInfo.Options.back();
    auto lastCond = equals(last.first);
    if (!lastCond) {
      return lastCond.takeError();
    }
    llvm::Value* defaultVal;
    if (matchInfo.Default) {
      auto val = std::get<1>(matchInfo.Default.value())->codegen(builder);
      if (!val) {
        return val.takeError();
      }
      defaultVal = *val;
    } else {
      defaultVal = llvm::Constant::getNullValue(matchInfo.Subject->getType());
    }
    auto lastVal = std::get<1>(last.second)->codegen(builder);
    if (!lastVal) {
      return lastVal.takeError();
    }

    auto select = builder.CreateSelect(*lastCond, *lastVal, defaultVal);
    for (size_t i = matchInfo.Options.size() - 1; i >= 1; --i) {
      const auto& opt = matchInfo.Options[i - 1];
      auto cmp = equals(opt.first);
      if (!cmp) {
        return cmp.takeError();
      }
      auto val = std::get<1>(opt.second)->codegen(builder);
      if (!val) {
        return val.takeError();
      }
      select = builder.CreateSelect(*cmp, *val, select);
    }
    return select;
  }

  inline static bool classof(const Factor* const factor) {
    return factor->getKind() == kMatchExpr;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace factors
} // namespace whack::codegen::expressions

#endif // WHACK_MATCHEXPR_HPP
