# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)

project(nearby CXX)

option(nearby_USE_LOCAL_PROTOBUF
       "Use local copy of protobuf library and compiler" OFF)

option(nearby_USE_LOCAL_ABSL
       "Use local copy of abseil-cpp library" OFF)

# target_sources() may convert relative paths to absolute
cmake_policy(SET CMP0076 NEW)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/proto_defs.cmake)
include(cmake/local_build_setup.cmake)

if (nearby_USE_LOCAL_PROTOBUF)
  include(cmake/local_build_protobuf.cmake)
endif()

include(cmake/local_setup_smhasher.cmake)

find_package(Protobuf REQUIRED)

enable_testing()

if (NOT TARGET ukey2)
add_subdirectory(third_party/ukey2)
endif()
if (NOT TARGET gtest)
add_subdirectory(third_party/gtest)
endif()
if (nearby_USE_LOCAL_ABSL)
  if (NOT TARGET absl::base)
    add_subdirectory(third_party/absl)
  endif()
else()
  find_package(absl REQUIRED)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cpp)

add_subdirectory(cpp/core)
add_subdirectory(cpp/platform)
add_subdirectory(proto)
