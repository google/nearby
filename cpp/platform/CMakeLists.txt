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

add_library(platform_utils STATIC
)

target_sources(platform_utils
  PRIVATE
    base64_utils.cc
    cancelable_alarm.cc
    pipe.cc
    prng.cc
    reliability_utils.cc
  PUBLIC
    base64_utils.h
    cancelable_alarm.h
    pipe.h
    prng.h
    reliability_utils.h
    synchronized.h
)

target_link_libraries(platform_utils
  PUBLIC
    platform_api
    platform_types
    platform_impl_g3
    platform_impl_shared_posix_lock
    platform_impl_shared_posix_condition_variable
    absl::strings
)

add_library(platform_types STATIC)

target_sources(platform_types
  PUBLIC
    byte_array.h
    callable.h
    cancelable.h
    container_of.h
    exception.h
    logging.h
    ptr.h
    runnable.h
)

target_link_libraries(platform_types
  PUBLIC
    absl::base
    absl::strings
)

add_executable(platform_test
  byte_array_test.cc
  container_of_test.cc
  exception_test.cc
  pipe_test.cc
  prng_test.cc
  ptr_test.cc
)

target_link_libraries(platform_test
  PUBLIC
    absl::base
    absl::strings
    absl::time
    gmock
    gtest
    gtest_main
    platform_api
    platform_impl_g3
    platform_types
    platform_utils
)

add_test(
  NAME platform_test
  COMMAND platform_test
)

add_subdirectory(api)
add_subdirectory(impl/g3)
add_subdirectory(impl/shared)
add_subdirectory(port)
