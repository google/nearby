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

add_library(platform_impl_shared_posix_lock STATIC)

target_sources(platform_impl_shared_posix_lock
  PRIVATE
    "posix_lock.cc"
  PUBLIC
    "posix_lock.h"
)

target_include_directories(platform_impl_shared_posix_lock
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(platform_impl_shared_posix_lock
  PUBLIC
    platform_api
    platform_types
)

add_library(platform_impl_shared_posix_condition_variable STATIC)

target_sources(platform_impl_shared_posix_condition_variable
  PRIVATE
    "posix_condition_variable.cc"
  PUBLIC
    "posix_condition_variable.h"
)

target_include_directories(platform_impl_shared_posix_condition_variable
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_shared_posix_condition_variable
  PUBLIC
    platform_api
    platform_impl_shared_posix_lock
    platform_types
    absl::raw_logging_internal
)

add_library(platform_impl_shared_file STATIC)

target_sources(platform_impl_shared_file
  PRIVATE
    "file_impl.cc"
  PUBLIC
    "file_impl.h"
)

target_include_directories(platform_impl_shared_file
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_shared_file
  PUBLIC
    platform_api
    platform_types
)

add_library(platform_impl_shared_atomic_boolean STATIC)

target_sources(platform_impl_shared_atomic_boolean
  PUBLIC
    "atomic_boolean_impl.h"
)

target_include_directories(platform_impl_shared_atomic_boolean
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_shared_atomic_boolean
  PUBLIC
    platform_api
    platform_types
)

set_target_properties(platform_impl_shared_atomic_boolean
  PROPERTIES
    LINKER_LANGUAGE CXX
)

add_subdirectory(sample)
