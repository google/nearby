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

cc_library(
    name = "types",
    srcs = [
        "log_message.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
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
    ],
    deps = [
        "//platform/api:types",
        "//platform/base",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "server_sync.h",
        "webrtc.h",
        "wifi.h",
        "wifi_lan.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//platform/api:comm",
        "//platform/base",
    ],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//platform/api:types",
        "//platform/base",
        "//absl/strings",
        "//openssl:crypto",
    ],
)

cc_library(
    name = "windows",
    srcs = [
        "platform.cc",
    ],
    visibility = [
        "//location/nearby/connections/windows:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//platform/api:comm",
        "//platform/api:platform",
        "//platform/api:types",
    ],
)

cc_test(
    name = "impl_test",
    size = "small",
    srcs = [
        "crypto_test.cc",
    ],
    deps = [
        ":comm",
        ":crypto",
        ":types",
        "//platform/api:types",
        "//testing/base/public:gunit_main",
    ],
)
