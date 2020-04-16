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

add_library(platform_impl_default STATIC)

target_sources(platform_impl_default
  PRIVATE
    default_platform.cc
  PUBLIC
    default_platform.h
)

target_include_directories(platform_impl_default
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_default
  PUBLIC
    platform_api
    platform_impl_default_cond_var
    platform_impl_default_lock
    platform_types
)

add_library(platform_impl_default_lock STATIC)

target_sources(platform_impl_default_lock
  PRIVATE
    default_lock.cc
  PUBLIC
    default_lock.h
)

target_link_libraries(platform_impl_default_lock
  PUBLIC
    platform_api
)

add_library(platform_impl_default_cond_var STATIC)

target_sources(platform_impl_default_cond_var
  PRIVATE
    default_condition_variable.cc
  PUBLIC
    default_condition_variable.h
)

target_link_libraries(platform_impl_default_cond_var
  PUBLIC
    platform_api
    platform_impl_default_lock
    platform_types
)
