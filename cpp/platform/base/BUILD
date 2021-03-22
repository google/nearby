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
        "feature_flags.h",
        "input_stream.h",
        "listeners.h",
        "nsd_service_info.h",
        "output_stream.h",
        "payload_id.h",
        "prng.h",
        "runnable.h",
        "socket.h",
        "types.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform:__subpackages__",
        "//platform/api:__subpackages__",
    ],
    deps = [
        "//absl/container:flat_hash_map",
        "//absl/meta:type_traits",
        "//absl/strings",
        "//absl/strings:str_format",
        "//absl/synchronization",
        "//absl/time",
    ],
)

cc_library(
    name = "util",
    srcs = [
        "base_input_stream.cc",
        "base_pipe.cc",
        "byte_utils.cc",
    ],
    hdrs = [
        "base_input_stream.h",
        "base_mutex_lock.h",
        "base_pipe.h",
        "byte_utils.h",
    ],
    visibility = [
        "//core:__subpackages__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":base",
        "//platform/api:types",
        "//absl/base:core_headers",
        "//absl/strings:str_format",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform:__subpackages__",
    ],
    deps = [
        "//platform/api:platform",
        "//platform/api:types",
    ],
)

cc_library(
    name = "cancellation_flag",
    srcs = [
        "cancellation_flag.cc",
    ],
    hdrs = [
        "cancellation_flag.h",
        "cancellation_flag_listener.h",
    ],
    visibility = [
        "//core/internal:__subpackages__",
        "//platform/api:__subpackages__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":base",
        ":util",
        "//absl/container:flat_hash_set",
        "//absl/synchronization",
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
        "//core:__subpackages__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":base",
        ":logging",
        "//platform/api:comm",
        "//platform/public:types",
        "//absl/container:flat_hash_map",
        "//absl/strings",
    ],
)

cc_test(
    name = "platform_base_test",
    srcs = [
        "bluetooth_utils_test.cc",
        "byte_array_test.cc",
        "feature_flags_test.cc",
        "prng_test.cc",
    ],
    deps = [
        ":base",
        ":test_util",
        "//platform/impl/g3",  # build_cleaner: keep
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "platform_util_test",
    srcs = [
        "byte_utils_test.cc",
    ],
    deps = [
        ":base",
        ":util",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "cancellation_flag_test",
    srcs = [
        "cancellation_flag_test.cc",
    ],
    deps = [
        ":base",
        ":cancellation_flag",
        ":test_util",
        "//platform/impl/g3",  # build_cleaner: keep
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
