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
#ifndef WHACK_RETURNSTMT_HPP
#define WHACK_RETURNSTMT_HPP

#pragma once

namespace whack::codegen::stmts {

class Return final : public Stmt {
public:
  explicit Return(const mpc_ast_t* const ast)
      : Stmt(kReturn), state_{ast->state} {
    if (ast->children_num > 1) {
      exprList_ = expressions::getExprList(ast->children[1]);
    }
  }

  llvm::Error codegen(llvm::IRBuilder<>& builder) const final {
    if (exprList_.empty()) {
      builder.CreateRetVoid();
    } else if (exprList_.size() == 1) {
      auto expr = exprList_.back()->codegen(builder);
      if (!expr) {
        return expr.takeError();
      }
      auto ret = expressions::getLoadedValue(builder, *expr);
      if (!ret) {
        return ret.takeError();
      }
      builder.CreateRet(*ret);
    } else {
      small_vector<llvm::Value*> values;
      for (const auto& expression : exprList_) {
        auto expr = expression->codegen(builder);
        if (!expr) {
          return expr.takeError();
        }
        auto val = expressions::getLoadedValue(builder, *expr);
        if (!val) {
          return val.takeError();
        }
        values.push_back(*val);
      }
      builder.CreateAggregateRet(values.data(),
                                 static_cast<unsigned>(values.size()));
    }
    return llvm::Error::success();
  }

  inline static bool classof(const Stmt* const stmt) {
    return stmt->getKind() == kReturn;
  }

private:
  const mpc_state_t state_;
  small_vector<expr_t> exprList_;
};

} // namespace whack::codegen::stmts

#endif // WHACK_RETURNSTMT_HPP
