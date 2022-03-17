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
        "log_message.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "bluetooth_adapter.h",
        "cancelable.h",
        "condition_variable.h",
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
        "utils.h",
    ],
    deps = [
        "//base",
        "//base:stringprintf",
        "//internal/platform:base",
        "//internal/platform/implementation:types",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
        "condition_variable.h",
        "executor.h",
        "mutex.h",
        "runner.h",
        "scheduled_executor.h",
        "server_sync.h",
        "submittable_executor.h",
        "thread_pool.h",
        "webrtc.h",
        "wifi.h",
        "wifi_lan.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/windows/generated:types",
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
        "bluetooth_adapter.cc",
        "bluetooth_classic_device.cc",
        "bluetooth_classic_medium.cc",
        "bluetooth_classic_server_socket.cc",
        "bluetooth_classic_socket.cc",
        "executor.cc",
        "platform.cc",
        "scheduled_executor.cc",
        "submittable_executor.cc",
        "system_clock.cc",
        "thread_pool.cc",
        "utils.cc",
        "wifi_lan_medium.cc",
        "wifi_lan_server_socket.cc",
        "wifi_lan_socket.cc",
    ],
    copts = ["-Ithird_party/nearby/internal/platform/implementation/windows/generated"],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    visibility = [
        "//location/nearby/cpp/sharing:__subpackages__",
        "//connections/clients/windows:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "//internal/platform/implementation/windows/generated:types",
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
        "//connections/clients/windows:__subpackages__",
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
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "executor_test.cc",
        "platform_test.cc",
        "scheduled_executor_test.cc",
        "submittable_executor_test.cc",
    ],
    copts = ["-Iplatform/implementation/windows/generated -DCORE_ADAPTER_DLL"],
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
        "@com_google_googletest//:gtest_main",
    ],
)
