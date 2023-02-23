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
    name = "types",
    srcs = [
        "device_info.cc",
        "log_message.cc",
        "timer.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "bluetooth_adapter.h",
        "condition_variable.h",
        "device_info.h",
        "executor.h",
        "future.h",
        "input_file.h",
        "listenable_future.h",
        "log_message.h",
        "mutex.h",
        "output_file.h",
        "scheduled_executor.h",
        "settable_future.h",
        "submittable_executor.h",
        "timer.h",
        "utils.h",
    ],
    copts = ["-Ithird_party/nearby/internal/platform/implementation/windows/generated"],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    deps = [
        "//base",
        "//base:stringprintf",
        "//internal/base:bluetooth_address",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/windows/generated:types",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "ble_gatt_client.h",
        "ble_medium.h",
        "ble_peripheral.h",
        "ble_socket.h",
        "ble_v2.h",
        "ble_v2_peripheral.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
        "bluetooth_pairing.h",
        "condition_variable.h",
        "executor.h",
        "file.h",
        "file_path.h",
        "http_loader.h",
        "mutex.h",
        "scheduled_executor.h",
        "server_sync.h",
        "submittable_executor.h",
        "thread_pool.h",
        "webrtc.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/windows/generated:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "mediums_api",
    hdrs = ["mediums_api.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/platform/implementation/windows/mediums_manager:types",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:types",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "windows",
    srcs = [
        "ble_gatt_client.cc",
        "ble_medium.cc",
        "ble_socket.cc",
        "ble_v2.cc",
        "ble_v2_peripheral.cc",
        "bluetooth_adapter.cc",
        "bluetooth_classic_device.cc",
        "bluetooth_classic_medium.cc",
        "bluetooth_classic_server_socket.cc",
        "bluetooth_classic_socket.cc",
        "bluetooth_pairing.cc",
        "executor.cc",
        "file.cc",
        "file_path.cc",
        "http_loader.cc",
        "platform.cc",
        "scheduled_executor.cc",
        "submittable_executor.cc",
        "system_clock.cc",
        "thread_pool.cc",
        "utils.cc",
        "wifi_direct_medium.cc",
        "wifi_direct_server_socket.cc",
        "wifi_direct_socket.cc",
        "wifi_hotspot_medium.cc",
        "wifi_hotspot_server_socket.cc",
        "wifi_hotspot_socket.cc",
        "wifi_lan_medium.cc",
        "wifi_lan_server_socket.cc",
        "wifi_lan_socket.cc",
        "wifi_medium.cc",
    ],
    # This is the temporary solution to solve compilation error of Win32 WFDxxx() related API.
    # WFD API is only support after _WIN32_WINNT_WIN8, but the current lexan _WIN32_WINNT is set to _WIN32_WINNT_WIN7
    copts = ["-Ithird_party/nearby/internal/platform/implementation/windows/generated -Ithird_party/nearby/internal/platform/implementation/windows/json -D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10"],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//location/nearby:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "//internal/platform/implementation/windows/generated:types",
        "//internal/platform/implementation/windows/json:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "test_utils",
    srcs = [
        "test_utils.cc",
    ],
    hdrs = [
        "test_data.h",
        "test_utils.h",
    ],
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
    deps = [
        "//internal/platform:base",
    ],
)

cc_test(
    name = "impl_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "ble_medium_test.cc",
        "ble_v2_peripheral_test.cc",
        "ble_v2_test.cc",
        "bluetooth_adapter_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "device_info_test.cc",
        "executor_test.cc",
        "file_path_test.cc",
        "http_loader_test.cc",
        "scheduled_executor_test.cc",
        "submittable_executor_test.cc",
        "thread_pool_test.cc",
        "timer_test.cc",
        "utils_test.cc",
    ],
    copts = ["-Ithird_party/nearby/internal/platform/implementation/windows/generated -DCORE_ADAPTER_DLL"],
    deps = [
        ":comm",
        ":crypto",
        ":test_utils",
        ":types",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation/windows",
        "//internal/platform/implementation/windows/generated:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
