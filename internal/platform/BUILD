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

load("//tools/build_defs/cc:cc_fake_binary.bzl", "cc_fake_binary")

licenses(["notice"])

cc_library(
    name = "base",
    srcs = [
        "base64_utils.cc",
        "bluetooth_utils.cc",
        "input_stream.cc",
        "nsd_service_info.cc",
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
    copts = ["-DCORE_ADAPTER_DLL"],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/platform:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
        "//third_party/nearby/cpp/cal:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
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
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        ":base",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/platform:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
    ],
    deps = [
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_google_glog//:glog",
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
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        ":base",
        ":util",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "error_code_recorder",
    srcs = [
        "error_code_recorder.cc",
    ],
    hdrs = [
        "error_code_params.h",
        "error_code_recorder.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        ":logging",
        "//proto:connections_enums_cc_proto",
        "//proto/errorcode:error_code_enums_cc_proto",
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
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
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
    defines = ["NO_WEBRTC"],
    deps = [
        ":base",
        ":test_util",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "platform_util_test",
    srcs = [
        "byte_utils_test.cc",
    ],
    defines = ["NO_WEBRTC"],
    deps = [
        ":base",
        ":util",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cancellation_flag_test",
    srcs = [
        "cancellation_flag_test.cc",
    ],
    defines = ["NO_WEBRTC"],
    deps = [
        ":base",
        ":cancellation_flag",
        ":test_util",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "error_code_recorder_test",
    srcs = [
        "error_code_recorder_test.cc",
    ],
    defines = ["NO_WEBRTC"],
    deps = [
        ":error_code_recorder",
        ":test_util",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "monitored_runnable.cc",
        "pending_job_registry.cc",
        "pipe.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "cancelable.h",
        "cancelable_alarm.h",
        "cancellable_task.h",
        "condition_variable.h",
        "core_config.h",
        "count_down_latch.h",
        "crypto.h",
        "file.h",
        "future.h",
        "lockable.h",
        "logging.h",
        "monitored_runnable.h",
        "multi_thread_executor.h",
        "mutex.h",
        "mutex_lock.h",
        "pending_job_registry.h",
        "pipe.h",
        "scheduled_executor.h",
        "settable_future.h",
        "single_thread_executor.h",
        "submittable_executor.h",
        "system_clock.h",
        "thread_check_callable.h",
        "thread_check_runnable.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation/ios:__subpackages__",
        "//internal/platform/implementation/windows:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        ":util",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/time",
        "@com_google_glog//:glog",
    ],
)

cc_library(
    name = "comm",
    srcs = [
        "ble.cc",
        "ble_v2.cc",
        "bluetooth_classic.cc",
        "file.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "wifi_lan.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/platform:__pkg__",
    ],
    deps = [
        ":logging",
        ":types",
        "//cpp/cal/base:types",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//third_party/nearby/cpp/cal/api:ble",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "ble_test.cc",
        "bluetooth_adapter_test.cc",
        "bluetooth_classic_test.cc",
        "cancelable_alarm_test.cc",
        "condition_variable_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "future_test.cc",
        "logging_test.cc",
        "multi_thread_executor_test.cc",
        "mutex_test.cc",
        "pipe_test.cc",
        "scheduled_executor_test.cc",
        "single_thread_executor_test.cc",
        "wifi_lan_test.cc",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    defines = ["NO_WEBRTC"],
    shard_count = 16,
    deps = [
        ":comm",
        ":logging",
        ":types",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_fake_binary(
    name = "thread_check_nocompile",
    srcs = ["thread_check_nocompile.cc"],
    deps = [
        ":types",
        "@com_google_absl//absl/time",
    ],
)

py_test(
    name = "thread_check_nocompile_test",
    size = "large",
    srcs = ["thread_check_nocompile_test.py"],
    data = ["thread_check_nocompile"],
    python_version = "PY3",
    srcs_version = "PY3",
    tags = ["non_compile_test"],
    deps = [
        "//testing/pybase",
        "//testing/pybase:fake_target_util",
    ],
)
