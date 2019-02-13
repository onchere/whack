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
#ifndef WHACK_MATCH_HPP
#define WHACK_MATCH_HPP

#pragma once

namespace whack::codegen {

static llvm::Expected<MatchInfo> getMatchInfo(const mpc_ast_t* const ast,
                                              llvm::IRBuilder<>& builder) {
  using namespace expressions;
  auto val = getExpressionValue(ast->children[1])->codegen(builder);
  if (!val) {
    return val.takeError();
  }
  auto subject = getLoadedValue(builder, *val);
  if (!subject) {
    return subject.takeError();
  }
  MatchInfo matchInfo;
  matchInfo.Subject = *subject;
  const auto type = matchInfo.Subject->getType();
  const auto tag = getOutermostAstTag(ast->children[3]);
  matchInfo.IsExpression =
      tag == "matchexprcase" ||
      (tag == "default" &&
       getOutermostAstTag(ast->children[5]) == "expression");
  small_vector<llvm::Value*> allOptions;

  for (auto i = 3; i < ast->children_num - 1; ++i) {
    const auto ref = ast->children[i];
    if (std::string_view(ref->contents) != "default") {
      small_vector<llvm::Value*> values;
      const auto expr = matchInfo.IsExpression ? ref->children[0] : ref;
      for (const auto& e : getExprList(expr)) {
        auto opt = e->codegen(builder);
        if (!opt) {
          return opt.takeError();
        }
        auto o = getLoadedValue(builder, *opt);
        if (!o) {
          return o.takeError();
        }
        const auto option = *o;
        if (option->getType() != type) {
          return error("invalid type for match option at line {}",
                       expr->state.row + 1);
        }
        if (std::find(allOptions.begin(), allOptions.end(), option) !=
            allOptions.end()) {
          return error("duplicate option for match at line {}",
                       expr->state.row + 1);
        }
        values.push_back(option);
        allOptions.push_back(option);
      }
      const auto res =
          matchInfo.IsExpression ? ref->children[2] : ast->children[i + 2];
      using res_t = MatchInfo::match_res_t;
      matchInfo.Options.emplace_back(
          std::make_pair(std::move(values), matchInfo.IsExpression
                                                ? res_t{getExpressionValue(res)}
                                                : res_t{stmts::getStmt(res)}));
      if (matchInfo.IsExpression) {
        if (std::string_view(ast->children[i + 1]->contents) == ";") {
          ++i;
        }
      } else {
        i += 2;
      }
    } else {
      if (matchInfo.IsExpression) {
        matchInfo.Default = getExpressionValue(ast->children[i + 2]);
      } else {
        matchInfo.Default = stmts::getStmt(ast->children[i + 2]);
      }
      break;
    }
  }
  return matchInfo;
}

namespace stmts {

class Match final : public Stmt {
public:
  explicit constexpr Match(const mpc_ast_t* const ast) noexcept
      : Stmt(kMatch), ast_{ast} {}

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    auto info = getMatchInfo(ast_, builder);
    if (!info) {
      return info.takeError();
    }
    const auto& matchInfo = *info;
    const auto func = builder.GetInsertBlock()->getParent();
    auto& ctx = func->getContext();
    auto defaultBlock = llvm::BasicBlock::Create(ctx, "default", func);
    const auto contBlock = llvm::BasicBlock::Create(ctx, "block", func);
    const auto switcher = builder.CreateSwitch(matchInfo.Subject, defaultBlock,
                                               matchInfo.Options.size());
    for (const auto& [alternatives, stmt] : matchInfo.Options) {
      auto caseBlock = llvm::BasicBlock::Create(ctx, "case", func);
      for (const auto& value : alternatives) {
        switcher->addCase(llvm::dyn_cast<llvm::ConstantInt>(value), caseBlock);
      }
      caseBlock->moveBefore(contBlock);
      builder.SetInsertPoint(caseBlock);
      const auto& s = std::get<0>(stmt);
      if (auto err = s->codegen(builder)) {
        return err;
      }
      if (auto err = s->runScopeExit(builder)) {
        return err;
      }
      caseBlock = builder.GetInsertBlock();
      if (!caseBlock->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }
    }

    builder.SetInsertPoint(defaultBlock);
    if (matchInfo.Default) {
      const auto& defaultStmt = std::get<0>(matchInfo.Default.value());
      if (auto err = defaultStmt->codegen(builder)) {
        return err;
      }
      if (auto err = defaultStmt->runScopeExit(builder)) {
        return err;
      }
      defaultBlock = builder.GetInsertBlock();
      if (!defaultBlock->back().isTerminator()) {
        builder.CreateBr(contBlock);
      }
    } else {
      builder.CreateBr(contBlock);
    }

    builder.SetInsertPoint(contBlock);
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kMatch;
  }

private:
  const mpc_ast_t* const ast_;
};

} // end namespace stmts
} // end namespace whack::codegen

#endif // WHACK_MATCH_HPP
