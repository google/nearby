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
        "device_info.h",
        "executor.h",
        "future.h",
        "input_file.h",
        "listenable_future.h",
        "log_message.h",
        "mutex.h",
        "output_file.h",
        "preferences_manager.h",
        "scheduled_executor.h",
        "settable_future.h",
        "submittable_executor.h",
        "system_clock.h",
        "timer.h",
    ],
    visibility = [
        "//connections/implementation/analytics:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal/crypto_cros:__pkg__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
        "//internal/preferences:__subpackages__",
        "//internal/test:__subpackages__",
        "//location/nearby/analytics/cpp:__subpackages__",
        "//location/nearby/cpp/common:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//presence:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        "//internal/crypto_cros",
        "//internal/platform:base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "credential_callbacks.h",
        "credential_storage.h",
        "http_loader.h",
        "server_sync.h",
        "webrtc.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    copts = ["-DNO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//fastpair/internal:__pkg__",
        "//fastpair/internal/mediums:__pkg__",
        "//internal/network:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
        "//presence/implementation:__subpackages__",
    ],
    deps = [
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:uuid",
        "//internal/proto:credential_cc_proto",
        "//internal/proto:local_credential_cc_proto",
        # TODO: Support WebRTC
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "platform",
    hdrs = [
        "platform.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal:__subpackages__",
        "//internal/network:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
        "//location/nearby/analytics/cpp:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        ":comm",
        ":types",
        "//internal/platform:base",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
