#
# Copyright 2018 Onchere Bironga
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
cmake_minimum_required(VERSION 3.10)
project(whack VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE "Release")
set(LLVM_INSTALL_DIR "C:/code/llvm-6.0.1.src")
set(FOLLY_INSTALL_DIR "C:/code/folly-2018.08.20.00")
set(SPDLOG_INSTALL_DIR "C:/code/spdlog-1.1.0")
set(CMAKE_CXX_FLAGS "-Wall -DFOLLY_NO_CONFIG -L${LLVM_INSTALL_DIR}/build/bin -lLLVM")

include_directories(whack PUBLIC "${LLVM_INSTALL_DIR}/build/include")
include_directories(whack PUBLIC "${LLVM_INSTALL_DIR}/include")
include_directories(whack PUBLIC "${SPDLOG_INSTALL_DIR}/include")
include_directories(whack PUBLIC "${FOLLY_INSTALL_DIR}")

# TODO Run ../scripts/parsers.py and/or keywords.py

add_executable(whack mpc/mpc.c main.cpp)
