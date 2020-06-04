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

add_library(platform_impl_g3 STATIC)

target_sources(platform_impl_g3
  PRIVATE
    "atomic_reference_impl.h"
    "platform.cc"
    "settable_future_impl.h"
    "system_clock_impl.h"
)

target_include_directories(platform_impl_g3
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_g3
  PUBLIC
    "platform_types"
    "platform_api"
    "platform_impl_shared_atomic_boolean"
    "platform_impl_shared_file"
    "platform_impl_shared_posix_condition_variable"
    "platform_impl_shared_posix_lock"
    "platform_port_string"
    "absl::base"
    "absl::synchronization"
    "absl::time"
)
