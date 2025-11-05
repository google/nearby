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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# Placeholder: load py_test

licenses(["notice"])

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_absl//absl/log:vlog_is_on",
    ],
)

cc_library(
    name = "base",
    srcs = [
        "base64_utils.cc",
        "input_stream.cc",
        "prng.cc",
    ],
    hdrs = [
        "base64_utils.h",
        "bluetooth_utils.h",
        "byte_array.h",
        "callable.h",
        "exception.h",
        "expected.h",
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
    visibility = [
        "//:__subpackages__",
        "//chrome/chromeos/assistant/data_migration/lib:__pkg__",
        "//connections:partners",
    ],
    deps = [
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "util",
    srcs = [
        "byte_utils.cc",
        "stream_reader.cc",
        "stream_writer.cc",
    ],
    hdrs = [
        "byte_utils.h",
        "stream_reader.h",
        "stream_writer.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//connections:partners",
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        ":base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:str_format",
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
    name = "error_code_recorder",
    srcs = [
        "error_code_recorder.cc",
    ],
    hdrs = [
        "error_code_params.h",
        "error_code_recorder.h",
    ],
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
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "connection_info",
    srcs = [
        "ble_connection_info.cc",
        "bluetooth_connection_info.cc",
        "connection_info.cc",
        "wifi_lan_connection_info.cc",
    ],
    hdrs = [
        "ble_connection_info.h",
        "bluetooth_connection_info.h",
        "connection_info.h",
        "wifi_lan_connection_info.h",
    ],
    visibility = [
        "//connections/implementation:__pkg__",
        "//connections/v3:__pkg__",
        "//internal/interop:__pkg__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":logging",
        ":mac_address",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "blocking_queue_stream.cc",
        "clock_impl.cc",
        "device_info_impl.cc",
        "monitored_runnable.cc",
        "pending_job_registry.cc",
        "pipe.cc",
        "task_runner_impl.cc",
        "timer_impl.cc",
    ],
    hdrs = [
        "array_blocking_queue.h",
        "atomic_boolean.h",
        "atomic_reference.h",
        "blocking_queue_stream.h",
        "borrowable.h",
        "cancelable.h",
        "cancelable_alarm.h",
        "cancellable_task.h",
        "clock.h",
        "clock_impl.h",
        "condition_variable.h",
        "count_down_latch.h",
        "crypto.h",
        "device_info.h",
        "device_info_impl.h",
        "direct_executor.h",
        "file.h",
        "future.h",
        "lockable.h",
        "monitored_runnable.h",
        "multi_thread_executor.h",
        "mutex.h",
        "mutex_lock.h",
        "pending_job_registry.h",
        "pipe.h",
        "scheduled_executor.h",
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
    visibility = [
        "//:__subpackages__",
        "//location/nearby/apps:__subpackages__",
        "//location/nearby/cpp:__subpackages__",
        "//location/nearby/sharing/sdk:__subpackages__",
        "//location/nearby/testing/nearby_native:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        ":util",
        "//connections/implementation/flags:connections_flags",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/crypto_cros",
        "//internal/flags:nearby_flags",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "comm",
    srcs = [
        "awdl.cc",
        "ble.cc",
        "bluetooth_classic.cc",
        "credential_storage_impl.cc",
        "file.cc",
        "wifi_direct.cc",
        "wifi_hotspot.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "awdl.h",
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "credential_storage_impl.h",
        "webrtc.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    copts = [
        "-DCORE_ADAPTER_DLL",
        "-DNO_WEBRTC",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//internal/test:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        ":cancellation_flag",
        ":logging",
        ":mac_address",
        ":types",
        ":uuid",
        "//internal/base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:wifi_utils",
        # TODO: Support WebRTC
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "mac_address",
    srcs = ["mac_address.cc"],
    hdrs = ["mac_address.h"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
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
        "//connections:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":base",
        ":logging",
        ":mac_address",
        ":types",
        ":uuid",
        "//internal/base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:wifi_utils",
        "//internal/test",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "platform_base_test",
    srcs = [
        "byte_array_test.cc",
        "feature_flags_test.cc",
        "input_stream_test.cc",
        "prng_test.cc",
    ],
    deps = [
        ":base",
        ":test_util",
        "//internal/platform/implementation:platform_impl",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "platform_util_test",
    srcs = [
        "byte_utils_test.cc",
        "stream_reader_test.cc",
        "stream_writer_test.cc",
    ],
    deps = [
        ":base",
        ":util",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
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
        "//internal/platform/implementation:platform_impl",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "expected_test",
    srcs = ["expected_test.cc"],
    deps = [
        ":base",
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "error_code_recorder_test",
    srcs = [
        "error_code_recorder_test.cc",
    ],
    deps = [
        ":error_code_recorder",
        ":test_util",
        "//internal/platform/implementation:platform_impl",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "public_device_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "ble_connection_info_test.cc",
        "ble_test.cc",
        "blocking_queue_stream_test.cc",
        "bluetooth_adapter_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_connection_info_test.cc",
        "pipe_test.cc",
        "wifi_direct_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_lan_connection_info_test.cc",
        "wifi_lan_test.cc",
        "wifi_test.cc",
    ],
    deps = [
        ":base",
        ":cancellation_flag",
        ":comm",
        ":connection_info",
        ":logging",
        ":mac_address",
        ":test_util",
        ":types",
        ":uuid",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform_impl",
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "credential_storage_impl_test",
    srcs = ["credential_storage_impl_test.cc"],
    deps = [
        ":comm",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform_impl",
        "//internal/proto:credential_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "borrowable_test.cc",
        "cancelable_alarm_test.cc",
        "condition_variable_test.cc",
        "connection_info_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "direct_executor_test.cc",
        "file_test.cc",
        "future_test.cc",
        "multi_thread_executor_test.cc",
        "mutex_test.cc",
        "scheduled_executor_test.cc",
        "single_thread_executor_test.cc",
        "task_runner_impl_test.cc",
        "timer_impl_test.cc",
        "uuid_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":base",
        ":connection_info",
        ":logging",
        ":mac_address",
        ":test_util",
        ":types",
        ":uuid",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/crypto_cros",
        "//internal/platform/implementation:platform_impl",
        "//internal/platform/implementation:types",
        "//internal/test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mac_address_test",
    srcs = ["mac_address_test.cc"],
    deps = [
        ":mac_address",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_googletest//:gtest_main",
    ],
)
