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
        "//googlemac/iPhone/Shared/Nearby/Connections_v2:__subpackages__",
        "//platform/base:__pkg__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        "//platform/base",
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
        "//core/internal:__subpackages__",
        "//platform/base:__pkg__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform/base",
        "//platform/base:cancellation_flag",
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
        "//googlemac/iPhone/Shared/Nearby/Connections_v2:__subpackages__",
        "//platform/base:__pkg__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":comm",
        ":types",
        "//platform/base",
        "//absl/strings",
    ],
)
