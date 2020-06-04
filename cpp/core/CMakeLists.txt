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

add_library(core STATIC)

target_sources(core
  PUBLIC
    core.h
)

target_include_directories(core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/cpp
)

target_link_libraries(core
  PUBLIC
    core_internal
    platform_types
)

add_library(core_types STATIC)

target_sources(core_types
  PRIVATE
    payload.cc
    strategy.cc
  PUBLIC
    listeners.h
    options.h
    params.h
    payload.h
    status.h
    strategy.h
)

target_link_libraries(core_types
  PUBLIC
    platform_api
    platform_port_string
    platform_types
    platform_utils
)

add_executable(core_build_test
    check_compilation.cc
)

target_link_libraries(core_build_test
  PUBLIC
    absl::strings
    core
    core_types
    platform_impl_g3
    platform_impl_shared_file
    platform_impl_shared_posix_lock
    platform_impl_shared_sample
    platform_port_string
    platform_types
    platform_utils
)

add_subdirectory(internal)
