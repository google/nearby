load("@rules_cc//cc:cc_library.bzl", "cc_library")

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
        "preferences_manager.cc",
        "scheduled_executor.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "condition_variable.h",
        "device_info.h",
        "multi_thread_executor.h",
        "mutex.h",
        "preferences_manager.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
        "timer.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":preferences_repository",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:posix_mutex",
        "//internal/test",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:log_streamer",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_nisaba//nisaba/port:thread_pool",
        "@nlohmann_json//:json",
    ],
    alwayslink = 1,
)

cc_library(
    name = "comm",
    testonly = True,
    srcs = [
        "awdl.cc",
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
        "awdl.h",
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "credential_storage_impl.h",
        "socket_base.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":types",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:mac_address",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/proto:credential_cc_proto",
        "//third_party/webrtc/files/stable/webrtc/api:create_modular_peer_connection_factory",
        "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/api:scoped_refptr",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "comm_test",
    srcs = [
        "awdl_test.cc",
        "ble_v2_test.cc",
    ],
    deps = [
        ":comm",
        ":g3",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:test_util",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_nisaba//nisaba/port:thread_pool/fiber",
    ],
)

cc_library(
    name = "crypto",
    testonly = True,
    srcs = [
        "crypto.cc",
    ],
    visibility = ["//internal/platform/implementation:__subpackages__"],
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
        "//connections:partners",
        "//internal/account:__subpackages__",
        "//internal/auth:__subpackages__",
        "//internal/crypto:__subpackages__",
        "//internal/data:__subpackages__",
        "//internal/network:__subpackages__",
        "//internal/platform:__subpackages__",
        "//internal/preferences:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
        "//internal/test:__subpackages__",
        "//internal/weave:__subpackages__",
        "//location/nearby/cpp:__subpackages__",
        "//location/nearby/sharing/sdk:__subpackages__",
        "//presence:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_nisaba//nisaba/port:thread_pool",
    ],
)

cc_library(
    name = "preferences_repository",
    srcs = ["preferences_repository.cc"],
    hdrs = ["preferences_repository.h"],
    deps = [
        "//internal/base:file_path",
        "//internal/base:files",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@nlohmann_json//:json",
    ],
)
