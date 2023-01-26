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
        "os_name.h",
        "output_stream.h",
        "payload_id.h",
        "prng.h",
        "runnable.h",
        "socket.h",
        "types.h",
        "wifi_credential.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal:__pkg__",
        "//internal/platform:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//location/nearby/cpp/fastpair:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
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
        "//fastpair:__subpackages__",
        "//internal:__pkg__",
        "//internal/crypto:__subpackages__",
        "//internal/network:__subpackages__",
        "//internal/platform:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
        "//location/nearby/analytics/cpp:__subpackages__",
        "//presence:__subpackages__",
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
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "connection_info",
    srcs = [
        "ble_connection_info.cc",
        "bluetooth_connection_info.cc",
        "wifi_lan_connection_info.cc",
    ],
    hdrs = [
        "ble_connection_info.h",
        "bluetooth_connection_info.h",
        "connection_info.h",
        "wifi_lan_connection_info.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//internal:__pkg__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
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
    visibility = ["//connections/implementation:__subpackages__"],
    deps = [
        ":logging",
        "//proto:connections_enums_cc_proto",
        "//proto/errorcode:error_code_enums_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_library(
    name = "uuid",
    srcs = [
        "uuid.cc",
    ],
    hdrs = [
        "uuid.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings",
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
        "//fastpair:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        ":types",
        ":uuid",
        "//internal/platform/implementation:comm",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
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
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/synchronization",
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
        "device_info_impl.cc",
        "monitored_runnable.cc",
        "pending_job_registry.cc",
        "pipe.cc",
        "task_runner_impl.cc",
        "timer_impl.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "borrowable.h",
        "cancelable.h",
        "cancelable_alarm.h",
        "cancellable_task.h",
        "condition_variable.h",
        "count_down_latch.h",
        "crypto.h",
        "device_info.h",
        "device_info_impl.h",
        "direct_executor.h",
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
        "task_runner.h",
        "task_runner_impl.h",
        "thread_check_callable.h",
        "thread_check_runnable.h",
        "timer.h",
        "timer_impl.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal/platform/implementation/windows:__subpackages__",
        "//location/nearby/cpp/fastpair:__subpackages__",
        "//location/nearby/testing/nearby_native:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        ":util",
        "//internal/crypto",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
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
        "credential_storage_impl.cc",
        "file.cc",
        "wifi_direct.cc",
        "wifi_hotspot.cc",
        "wifi_lan.cc",
        "wifi_utils.cc",
    ],
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "credential_storage_impl.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
        "wifi_utils.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"] + select({
        "@platforms//cpu:arm": [
            "-DNO_WEBRTC",
        ],
        "//conditions:default": [],
    }),
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//location/nearby/cpp/fastpair:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        ":cancellation_flag",
        ":logging",
        ":types",
        ":uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "ble_connection_info_test.cc",
        "ble_test.cc",
        "ble_v2_test.cc",
        "bluetooth_adapter_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_connection_info_test.cc",
        "borrowable_test.cc",
        "cancelable_alarm_test.cc",
        "condition_variable_test.cc",
        "count_down_latch_test.cc",
        "credential_storage_impl_test.cc",
        "crypto_test.cc",
        "direct_executor_test.cc",
        "future_test.cc",
        "logging_test.cc",
        "multi_thread_executor_test.cc",
        "mutex_test.cc",
        "pipe_test.cc",
        "scheduled_executor_test.cc",
        "single_thread_executor_test.cc",
        "task_runner_impl_test.cc",
        "timer_impl_test.cc",
        "uuid_test.cc",
        "wifi_direct_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_lan_connection_info_test.cc",
        "wifi_lan_test.cc",
        "wifi_test.cc",
        "wifi_utils_test.cc",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    defines = ["NO_WEBRTC"],
    shard_count = 16,
    deps = [
        ":base",
        ":comm",
        ":connection_info",
        ":logging",
        ":test_util",
        ":types",
        ":uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/proto:credential_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
