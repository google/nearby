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
        "log_message.h",
        "multi_thread_executor.h",
        "mutex.h",
        "pipe.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//base",
        "//base:stringprintf",
        "//cpp/platform/api:platform",
        "//cpp/platform/api:types",
        "//cpp/platform/base",
        "//cpp/platform/base:util",
        "//cpp/platform/impl/shared:count_down_latch",
        "//cpp/platform/impl/shared:posix_mutex",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_nisaba//nisaba/port:thread_pool",
    ],
)

cc_library(
    name = "comm",
    testonly = True,
    srcs = [
        "ble.cc",
        "bluetooth_adapter.cc",
        "bluetooth_classic.cc",
        "webrtc.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "webrtc.h",
        "wifi_lan.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":types",
        "//cpp/platform/api:comm",
        "//cpp/platform/base",
        "//cpp/platform/base:cancellation_flag",
        "//cpp/platform/base:logging",
        "//cpp/platform/base:test_util",
        "//cpp/platform/impl/shared:count_down_latch",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "//webrtc/api:create_peerconnection_factory",  #buildcleaner: keep
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api/task_queue:default_task_queue_factory",
    ],
)

cc_library(
    name = "crypto",
    testonly = True,
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
    name = "g3",
    testonly = True,
    srcs = [
        "platform.cc",
    ],
    visibility = [
        "//cpp/core:__subpackages__",
        "//cpp/platform:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//cpp/platform/api:comm",
        "//cpp/platform/api:platform",
        "//cpp/platform/api:types",
        "//cpp/platform/base:test_util",
        "//cpp/platform/impl/shared:count_down_latch",
        "//cpp/platform/impl/shared:file",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)
