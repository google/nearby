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

package(default_visibility = [
    "//connections:__subpackages__",
    "//internal/platform/implementation/apple:__subpackages__",
    "//location/nearby:__subpackages__",
    "//sharing:__subpackages__",
])

objc_library(
    name = "apple",
    srcs = [
        "crypto.mm",
        "device_info.mm",
        "multi_thread_executor.mm",
        "platform.mm",
        "preferences_manager.mm",
        "scheduled_executor.mm",
        "timer.mm",
        "wifi_lan.mm",
    ],
    hdrs = [
        "device_info.h",
        "multi_thread_executor.h",
        "preferences_manager.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
        "timer.h",
        "wifi_lan.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    features = ["-layering_check"],
    deps = [
        ":Platform_cc",
        ":Shared",
        ":ble_v2",
        # Required Reason API File: third_party/nearby/internal/platform/implementation/apple/preferences_manager.mm
        "//releasetools/apple/privacy/privacymanifests/requiredreasonsapi:user_defaults-user_defaults-read_write_app_data_ca92_1",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "@nlohmann_json//:json",
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/apple/Mediums",
        "//internal/platform/implementation/shared:file",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
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
        "ble_gatt_client.h",
        "ble_gatt_client.mm",
        "ble_gatt_server.h",
        "ble_gatt_server.mm",
        "ble_medium.h",
        "ble_medium.mm",
        "ble_peripheral.h",
        "ble_peripheral.mm",
        "ble_server_socket.h",
        "ble_server_socket.mm",
        "ble_socket.h",
        "ble_socket.mm",
        "ble_utils.mm",
        "bluetooth_adapter_v2.h",
        "bluetooth_adapter_v2.mm",
        "utils.mm",
    ],
    hdrs = [
        "ble.h",
        "ble_utils.h",
        "utils.h",
    ],
    # Prevent Objective-C++ headers from being pulled into swift.
    aspect_hints = ["//tools/build_defs/swift:no_module"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/apple/Mediums",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Central",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Peripheral",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
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
