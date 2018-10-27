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
#ifndef WHACK_FORMAT_HPP
#define WHACK_FORMAT_HPP

#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace whack {

template <typename T, typename... Args>
inline static constexpr auto format(T &&str, Args &&... args) {
  return fmt::format(std::forward<T>(str), std::forward<Args>(args)...);
}

// @todo llvm::ManagedStatic?
static auto console = spdlog::stdout_color_mt("whack");

template <typename T, typename... Args>
/*[[noreturn]]*/ inline static void fatal(T &&str, Args &&... args) {
  spdlog::get("whack")->critical(std::forward<T>(str),
                                 std::forward<Args>(args)...);
  /*exit(-1);*/
}

template <typename T, typename... Args>
inline static void warning(T &&str, Args &&... args) {
  spdlog::get("whack")->warn(std::forward<T>(str), std::forward<Args>(args)...);
}

} // end namespace whack

#endif // WHACK_FORMAT_HPP
