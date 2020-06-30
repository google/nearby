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
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "cancelable.h",
        "condition_variable.h",
        "count_down_latch.h",
        "crypto.h",
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
        "system_clock.h",
    ],
    visibility = [
        "//platform_v2/base:__pkg__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
        "//absl/base:core_headers",
        "//absl/strings",
        "//absl/time",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "server_sync.h",
        "webrtc.h",
        "wifi.h",
        "wifi_lan.h",
    ],
    visibility = [
        "//platform_v2/base:__pkg__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
        "//absl/strings",
        "//absl/types:optional",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "platform",
    hdrs = [
        "platform.h",
    ],
    visibility = [
        "//platform_v2/base:__pkg__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":comm",
        ":types",
        "//platform_v2/base",
        "//absl/strings",
    ],
)
