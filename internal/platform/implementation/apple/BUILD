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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_cc//cc:objc_library.bzl", "objc_library")

licenses(["notice"])

package(default_visibility = [
    "//ambient/nearby/testing/connection/mdc/ios:__subpackages__",
    "//connections:__subpackages__",
    "//googlemac/iPhone/Nearby:__subpackages__",
    "//internal/platform:__subpackages__",
    "//internal/preferences:__subpackages__",
    "//location/nearby:__subpackages__",
    "//sharing:__subpackages__",
])

objc_library(
    name = "comm",
    srcs = [
        "multi_thread_executor.mm",
    ],
    hdrs = [
        "multi_thread_executor.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/apple/Flags",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
    ],
)

objc_library(
    name = "apple",
    srcs = [
        "awdl.mm",
        "crypto.mm",
        "device_info.mm",
        "platform.mm",
        "preferences_manager.mm",
        "scheduled_executor.mm",
        "timer.mm",
        "webrtc.mm",
        "wifi_hotspot.mm",
        "wifi_lan.mm",
    ],
    hdrs = [
        "awdl.h",
        "device_info.h",
        "preferences_manager.h",
        "timer.h",
        "webrtc.h",
        "wifi.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    features = ["-layering_check"],
    deps = [
        ":GNCDevicePaths",
        ":comm",
        ":Platform_cc",
        ":Shared",
        ":ble_v2",
        ":app_lifecycle_monitor",
        ":network_utils",
        # Required Reason API File: third_party/nearby/internal/platform/implementation/apple/preferences_manager.mm
        "//releasetools/apple/privacy/privacymanifests/requiredreasonsapi:user_defaults-user_defaults-read_write_app_data_ca92_1",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "@nlohmann_json//:json",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/base:masker",
        "//internal/platform/implementation/apple/Mediums/Hotspot",
        "//internal/account",
        "//internal/crypto_cros",
        "//internal/platform/implementation:account_manager",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/proto:tachyon_cc_proto",
        "//third_party/webrtc/files/stable/webrtc/api/task_queue:default_task_queue_factory",
        "//third_party/webrtc/files/stable/webrtc/api:create_modular_peer_connection_factory",
        "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:checks",
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/apple/Mediums",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//internal/platform/implementation/shared:file",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//internal/platform:cancellation_flag",
    ] + select({
        "@platforms//os:platform_ios": [
            "//third_party/apple_frameworks:UIKit",
        ],
        "//conditions:default": [],
    }),
)

objc_library(
    name = "GNCNotificationCenter",
    hdrs = ["GNCNotificationCenter.h"],
    deps = [
        "//third_party/apple_frameworks:Foundation",
    ],
)

objc_library(
    name = "app_lifecycle_monitor",
    srcs = [
        "app_lifecycle_monitor.mm",
    ],
    hdrs = [
        "app_lifecycle_monitor.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    deps = [
        ":GNCNotificationCenter",
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:Foundation",
    ] + select({
        "@platforms//os:platform_ios": [
            "//third_party/apple_frameworks:UIKit",
        ],
        "//conditions:default": [],
    }),
)

objc_library(
    name = "ble_v2",
    srcs = [
        "ble_gatt_client.mm",
        "ble_gatt_server.mm",
        "ble_l2cap_server_socket.mm",
        "ble_l2cap_socket.mm",
        "ble_medium.mm",
        "ble_peripheral.mm",
        "ble_server_socket.mm",
        "ble_socket.mm",
        "ble_utils.mm",
        "utils.mm",
    ],
    hdrs = [
        "ble.h",
        "ble_gatt_client.h",
        "ble_gatt_server.h",
        "ble_l2cap_server_socket.h",
        "ble_l2cap_socket.h",
        "ble_medium.h",
        "ble_peripheral.h",
        "ble_server_socket.h",
        "ble_socket.h",
        "ble_utils.h",
        "utils.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    deps = [
        ":Shared",
        ":bluetooth_adapter_v2",
        ":comm",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:mac_address",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/apple/Flags",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//internal/platform/implementation/apple/Mediums/BLE",
        "//internal/platform/implementation/apple/Mediums/BLE/Sockets:Central",
        "//internal/platform/implementation/apple/Mediums/BLE/Sockets:Peripheral",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

objc_library(
    name = "bluetooth_adapter_v2",
    srcs = [
        "bluetooth_adapter_v2.mm",
    ],
    hdrs = [
        "bluetooth_adapter_v2.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    deps = [
        "//internal/platform:mac_address",
        "//internal/platform/implementation:comm",
        "//third_party/apple_frameworks:Foundation",
        "@com_google_absl//absl/strings",
    ],
)

objc_library(
    name = "Shared",
    srcs = [
        "GNCUtils.m",
    ],
    hdrs = [
        "GNCUtils.h",
    ],
    deps = [
        "//third_party/apple_frameworks:Foundation",
        "@aappleby_smhasher//:libmurmur3",
    ],
)

objc_library(
    name = "network_utils",
    srcs = [
        "network_utils.mm",
    ],
    hdrs = [
        "network_utils.h",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

objc_library(
    name = "os_log_sink",
    srcs = [
        "os_log_sink.mm",
    ],
    hdrs = [
        "os_log_sink.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    deps = [
        "//third_party/apple_frameworks:os",
        "@com_google_absl//absl/log:log_entry",
        "@com_google_absl//absl/log:log_sink",
        "@com_google_absl//absl/strings",
    ],
)

objc_library(
    name = "nearby_logger",
    srcs = [
        "nearby_logger.mm",
    ],
    hdrs = [
        "nearby_logger.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    deps = [
        ":os_log_sink",
        "@com_google_absl//absl/log:log_sink_registry",
    ],
)

objc_library(
    name = "GNCDevicePaths",
    srcs = ["GNCDevicePaths.m"],
    hdrs = ["GNCDevicePaths.h"],
    deps = [
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:Foundation",
    ],
)

cc_library(
    name = "Platform_cc",
    srcs = [
        "condition_variable.cc",
        "count_down_latch.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_uint32.h",
        "condition_variable.h",
        "count_down_latch.h",
        "mutex.h",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "Platform_cc_test",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_uint32_test.cc",
        "condition_variable_test.cc",
        "count_down_latch_test.cc",
        "mutex_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":Platform_cc",
        "//internal/platform/implementation/g3:crypto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_nisaba//nisaba/port:thread_pool/fiber",
    ],
)
