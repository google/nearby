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

load("//ads/util/non_compile:non_compile.bzl", "cc_with_non_compile_test")

cc_library(
    name = "base",
    srcs = [
        "base64_utils.cc",
        "bluetooth_utils.cc",
        "prng.cc",
    ],
    hdrs = [
        "base64_utils.h",
        "bluetooth_utils.h",
        "byte_array.h",
        "callable.h",
        "exception.h",
        "input_stream.h",
        "listeners.h",
        "output_stream.h",
        "payload_id.h",
        "prng.h",
        "runnable.h",
        "socket.h",
        "types.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
        "//platform_v2/api:__subpackages__",
    ],
    deps = [
        "//absl/meta:type_traits",
        "//absl/strings",
        "//absl/strings:str_format",
        "//absl/time",
    ],
)

cc_library(
    name = "util",
    srcs = [
        "base_input_stream.cc",
        "base_pipe.cc",
    ],
    hdrs = [
        "base_input_stream.h",
        "base_mutex_lock.h",
        "base_pipe.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":base",
        "//platform_v2/api:types",
        "//absl/base:core_headers",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = [
        "//platform_v2:__subpackages__",
    ],
    deps = [
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
    ],
)

cc_library(
    name = "test_util",
    testonly = True,
    srcs = [
        "medium_environment.cc",
    ],
    hdrs = [
        "medium_environment.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":base",
        ":logging",
        "//platform_v2/api:comm",
        "//platform_v2/public:types",
        "//absl/container:flat_hash_map",
        "//absl/strings",
    ],
)

cc_test(
    name = "platform_base_test",
    srcs = [
        "bluetooth_utils_test.cc",
        "byte_array_test.cc",
        "prng_test.cc",
    ],
    deps = [
        ":base",
        "//testing/base/public:gunit_main",
    ],
)

cc_with_non_compile_test(
    name = "exception_test",
    srcs = [
        "exception_test.cc",
    ],
    deps = [
        ":base",
        "//testing/base/public:gunit_main",
    ],
)
