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
    testonly = True,
    srcs = [
        "log_message.cc",
        "scheduled_executor.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "condition_variable.h",
        "device_info.h",
        "log_message.h",
        "multi_thread_executor.h",
        "mutex.h",
        "pipe.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
        "timer.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:util",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:posix_mutex",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_glog//:glog",
        "@com_google_nisaba//nisaba/port:thread_pool",
    ],
    alwayslink = 1,
)

cc_library(
    name = "comm",
    testonly = True,
    srcs = [
        "ble.cc",
        "ble_v2.cc",
        "bluetooth_adapter.cc",
        "bluetooth_classic.cc",
        "credential_storage_impl.cc",
        "wifi_direct.cc",
        "wifi_hotspot.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "credential_storage_impl.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = ["//visibility:private"],
    deps = [
        ":types",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/proto:credential_cc_proto",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:checks",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "crypto",
    testonly = True,
    srcs = [
        "crypto.cc",
    ],
    defines = ["NO_WEBRTC"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:types",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
    ],
    alwayslink = 1,
)

cc_library(
    name = "g3",
    testonly = True,
    srcs = [
        "platform.cc",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal:__pkg__",
        "//internal/network:__subpackages__",
        "//internal/platform:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
        "//location/nearby/cpp/fastpair:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//file/base:path",
        "//internal/platform:test_util",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)
