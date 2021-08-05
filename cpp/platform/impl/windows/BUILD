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
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "bluetooth_adapter.h",
        "cancelable.h",
        "condition_variable.h",
        "count_down_latch.h",
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
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        "//base",
        "//platform/api:types",
        "//platform/base",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "bluetooth_classic.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
        "server_sync.h",
        "webrtc.h",
        "wifi.h",
        "wifi_lan.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:private"],
    deps = [
        "//platform/api:comm",
        "//platform/base",
        "//platform/impl/windows/generated:types",
    ],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:private"],
    deps = [
        "//absl/strings",
        "//platform/api:types",
        "//platform/base",
        "//openssl:crypto",
    ],
)

cc_library(
    name = "windows",
    srcs = [
        "bluetooth_classic_device.cc",
        "bluetooth_classic_medium.cc",
        "bluetooth_classic_server_socket.cc",
        "bluetooth_classic_socket.cc",
        "platform.cc",
    ],
    hdrs = [
        "bluetooth_classic.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = ["-Ithird_party/nearby_connections/cpp/platform/impl/windows/generated"],
    visibility = [
        "//third_party/nearby_connections/windows:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//platform/api:comm",
        "//platform/api:platform",
        "//platform/api:types",
        "//platform/impl/shared:file",
        "//platform/impl/windows/generated:types",
        "//platform/public:types",
    ],
)

cc_library(
    name = "test_utils",
    srcs = [
        "test_utils.cc",
    ],
    hdrs = [
        "test_utils.h",
    ],
    visibility = [
        "//third_party/nearby_connections/windows:__subpackages__",
    ],
    deps = [
        "//platform/base",
    ],
)

cc_test(
    name = "impl_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "crypto_test.cc",
        "input_file_test.cc",
        "output_file_test.cc",
    ],
    copts = ["-Iplatform/impl/windows/generated"],
    deps = [
        ":comm",
        ":crypto",
        ":test_utils",
        ":types",
        "//testing/base/public:gunit_main",
        "//platform/api:platform",
        "//platform/base",
        "//platform/impl/windows",
        "//platform/impl/windows/generated:types",
        "//platform/public:logging",
    ],
)
