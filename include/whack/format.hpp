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
#ifndef WHACK_FORMAT_HPP
#define WHACK_FORMAT_HPP

#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>

namespace whack {

#define FORMAT_TPL                                                             \
  template <typename... Args, typename = std::enable_if_t<(sizeof...(Args))>>

FORMAT_TPL
inline static constexpr auto format(Args&&... args) {
  return fmt::format(std::forward<Args>(args)...);
}

inline static auto Log = spdlog::stderr_color_mt("whack");

FORMAT_TPL
inline static void fatal(Args&&... args) {
  Log->critical(std::forward<Args>(args)...);
}

FORMAT_TPL
inline static void warning(Args&&... args) {
  Log->warn(std::forward<Args>(args)...);
}

#undef FORMAT_TPL

} // end namespace whack

#endif // WHACK_FORMAT_HPP
