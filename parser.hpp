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
#ifndef WHACK_PARSER_HPP
#define WHACK_PARSER_HPP

#pragma once

#include "mpc/mpc.h"
#include <cassert>

namespace whack {

class Parser {
#include "parserlist.def"
public:
  explicit Parser(const std::string& grammarFileName) {
    auto err = mpca_lang_contents(MPCA_LANG_DEFAULT, grammarFileName.c_str(),
                                  parsers, nullptr);
    if (err != nullptr) {
      mpc_err_print(err);
      mpc_err_delete(err);
    } else {
      mpc_optimise(whack);
    }
  }

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;

  inline auto get() const {
    assert(whack && "parser not built!");
    return whack;
  }

  ~Parser() {
    constexpr static auto numParsers =
        std::tuple_size<decltype(std::tuple{parsers})>::value;
    mpc_cleanup(numParsers, parsers);
  }

#undef parsers
private:
#include "parsermembers.def"
};

} // end namespace whack

#endif // WHACK_PARSER_HPP
