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
        "//cpp/platform/api:types",
        "//cpp/platform/base",
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
        "//cpp/platform/api:comm",
        "//cpp/platform/base",
        "//cpp/platform/impl/windows/generated:types",
    ],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//cpp/platform/api:types",
        "//cpp/platform/base",
        "@com_google_absl//absl/strings",
        "@boringssl//:crypto",
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
    copts = ["-Ithird_party/nearby/cpp/platform/impl/windows/generated"],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    visibility = [
        "//third_party/nearby/windows:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//cpp/platform/api:comm",
        "//cpp/platform/api:platform",
        "//cpp/platform/api:types",
        "//cpp/platform/impl/shared:count_down_latch",
        "//cpp/platform/impl/shared:file",
        "//cpp/platform/impl/windows/generated:types",
        "//cpp/platform/public:types",
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
        "//third_party/nearby/windows:__subpackages__",
    ],
    deps = [
        "//cpp/platform/base",
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
        "scheduled_executor_test.cc",
        "submittable_executor_test.cc",
    ],
    copts = ["-Iplatform/impl/windows/generated -DCORE_ADAPTER_DLL"],
    deps = [
        ":comm",
        ":crypto",
        ":test_utils",
        ":types",
        "//cpp/platform/api:platform",
        "//cpp/platform/base",
        "//cpp/platform/impl/windows",
        "//cpp/platform/impl/windows/generated:types",
        "//cpp/platform/public:logging",
        "@com_google_googletest//:gtest_main","@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
    ],
)
