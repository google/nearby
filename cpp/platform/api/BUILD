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
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "bluetooth_adapter.h",
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
        "//cpp/platform/base:__pkg__",
        "//cpp/platform/impl:__subpackages__",
        "//cpp/platform/public:__pkg__",
    ],
    deps = [
        "//cpp/platform/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
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
        "//cpp/core/internal:__subpackages__",
        "//cpp/platform/base:__pkg__",
        "//cpp/platform/impl:__subpackages__",
        "//cpp/platform/public:__pkg__",
    ],
    deps = [
        "//connections/implementation/proto:offline_wire_formats_portable_proto",
        "//cpp/platform/base",
        "//cpp/platform/base:cancellation_flag",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "platform",
    hdrs = [
        "platform.h",
    ],
    visibility = [
        "//cpp/platform/base:__pkg__",
        "//cpp/platform/impl:__subpackages__",
        "//cpp/platform/public:__pkg__",
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
    ],
    deps = [
        ":comm",
        ":types",
        "//cpp/platform/base",
        "@com_google_absl//absl/strings",
    ],
)
