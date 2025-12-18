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

licenses(["notice"])

cc_library(
    name = "device_paths",
    srcs = [
        "device_paths.cc",
    ],
    hdrs = [
        "device_paths.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    linkopts = [
        "ole32.lib",
        "shell32.lib",
    ],
    tags = ["windows"],
    visibility = [
        "//location/nearby/apps/better_together/plugins/crashpad/windows:__pkg__",
        "//location/nearby/apps/better_together/plugins/logging/windows:__pkg__",
    ],
    deps = [
        "//internal/base:file_path",
        "//internal/base:files",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "device_info.cc",
        "scheduled_executor.cc",
        "session_manager.cc",
        "submittable_executor.cc",
        "task_scheduler.cc",
        "thread_pool.cc",
        "timer.cc",
        "utils.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "bluetooth_adapter.h",
        "condition_variable.h",
        "device_info.h",
        "executor.h",
        "mutex.h",
        "scheduled_executor.h",
        "session_manager.h",
        "submittable_executor.h",
        "task_scheduler.h",
        "thread_pool.h",
        "timer.h",
        "utils.h",
    ],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    tags = ["windows"],
    visibility = [
        "//sharing/internal/impl/windows:__pkg__",
    ],
    deps = [
        ":device_paths",
        ":string_utils",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/windows/generated:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:types",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "string_utils",
    srcs = [
        "string_utils.cc",
    ],
    hdrs = [
        "string_utils.h",
    ],
    tags = ["windows"],
    visibility = [
        "//:__subpackages__",
        "//location/nearby:__subpackages__",
    ],
    deps = [
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "network_info",
    srcs = [
        "network_info.cc",
    ],
    hdrs = [
        "network_info.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    tags = ["windows"],
    visibility = [
        "//:__subpackages__",
        "//location/nearby:__subpackages__",
    ],
    deps = [
        ":string_utils",
        ":wlan_client",
        "//internal/platform:logging",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "scoped_wlan_memory",
    hdrs = [
        "scoped_wlan_memory.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    tags = ["windows"],
    visibility = [
        "//:__subpackages__",
        "//location/nearby:__subpackages__",
    ],
)

cc_library(
    name = "wlan_client",
    srcs = [
        "wlan_client.cc",
    ],
    hdrs = [
        "wlan_client.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    tags = ["windows"],
    visibility = [
        "//:__subpackages__",
        "//location/nearby:__subpackages__",
    ],
    deps = [
        ":scoped_wlan_memory",
        "//internal/platform:logging",
    ],
)

cc_library(
    name = "windows",
    srcs = [
        "ble.cc",
        "ble_gatt_client.cc",
        "ble_gatt_server.cc",
        "ble_server_socket.cc",
        "ble_socket.cc",
        "bluetooth_adapter.cc",
        "bluetooth_classic_device.cc",
        "bluetooth_classic_medium.cc",
        "bluetooth_classic_server_socket.cc",
        "bluetooth_classic_socket.cc",
        "bluetooth_pairing.cc",
        "executor.cc",
        "file.cc",
        "file.h",
        "file_path.cc",
        "http_loader.cc",
        "nearby_client_socket.cc",
        "nearby_server_socket.cc",
        "platform.cc",
        "preferences_manager.cc",
        "preferences_repository.cc",
        "system_clock.cc",
        "webrtc.cc",
        "wifi_direct_medium.cc",
        "wifi_direct_server_socket.cc",
        "wifi_direct_socket.cc",
        "wifi_hotspot_medium.cc",
        "wifi_hotspot_native.cc",
        "wifi_hotspot_server_socket.cc",
        "wifi_hotspot_socket.cc",
        "wifi_intel.cc",
        "wifi_lan_mdns.cc",
        "wifi_lan_medium.cc",
        "wifi_lan_server_socket.cc",
        "wifi_lan_socket.cc",
        "wifi_medium.cc",
    ],
    hdrs = [
        "ble.h",
        "ble_gatt_client.h",
        "ble_gatt_server.h",
        "ble_server_socket.h",
        "ble_socket.h",
        "bluetooth_classic.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
        "bluetooth_pairing.h",
        "file_path.h",
        "http_loader.h",
        "nearby_client_socket.h",
        "nearby_server_socket.h",
        "preferences_manager.h",
        "preferences_repository.h",
        "webrtc.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_hotspot_native.h",
        "wifi_hotspot_server_socket.h",
        "wifi_hotspot_socket.h",
        "wifi_intel.h",
        "wifi_lan.h",
        "wifi_lan_mdns.h",
    ],
    # This is the temporary solution to solve compilation error of Win32 WFDxxx() related API.
    # WFD API is only support after _WIN32_WINNT_WIN8, but the current lexan _WIN32_WINNT is set to _WIN32_WINNT_WIN7
    copts = [
        "-DNO_INTEL_PIE",
        "-D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10",
        "-Wno-unused-variable",
        "-Wno-unused-value",
    ],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    tags = ["windows"],
    visibility = [
        "//chrome/chromeos/assistant/data_migration/lib:__pkg__",
        "//connections:__subpackages__",
        "//connections:partners",
        "//internal/platform:__subpackages__",
        "//location/nearby:__subpackages__",
        "//presence:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":crypto",  # build_cleaner: keep
        ":network_info",
        ":scoped_wlan_memory",
        ":socket_address",
        ":string_utils",
        ":types",
        ":wlan_client",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/mediums/ble:ble_advertisement_header",
        "//connections/implementation/mediums/ble:bloom_filter",
        "//internal/base",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/base:masker",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation:wifi_utils",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/windows/generated:types",
        "//third_party/intel/pie",
        "//third_party/webrtc/files/stable/webrtc/api:create_modular_peer_connection_factory",
        "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/api:rtc_error",
        "//third_party/webrtc/files/stable/webrtc/api:scoped_refptr",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:threading",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "socket_address",
    srcs = [
        "socket_address.cc",
    ],
    hdrs = [
        "socket_address.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    defines = [
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
        "WINVER=_WIN32_WINNT_WIN10",
    ],
    tags = ["windows"],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "test_utils",
    srcs = [
        "test_utils.cc",
    ],
    hdrs = [
        "test_data.h",
        "test_utils.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "impl_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "ble_gatt_server_test.cc",
        "ble_test.cc",
        "bluetooth_adapter_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "device_info_test.cc",
        "executor_test.cc",
        "file_path_test.cc",
        "http_loader_test.cc",
        "platform_test.cc",
        "preferences_manager_test.cc",
        "preferences_repository_test.cc",
        "scheduled_executor_test.cc",
        "submittable_executor_test.cc",
        "task_scheduler_test.cc",
        "thread_pool_test.cc",
        "timer_test.cc",
        "utils_test.cc",
        "webrtc_test.cc",
        "wifi_direct_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_medium_test.cc",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    tags = ["nozapfhahn"],
    deps = [
        ":crypto",
        ":test_utils",
        ":types",
        ":windows",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/windows/generated:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "string_utils_test",
    size = "small",
    timeout = "short",
    srcs = [
        "string_utils_test.cc",
    ],
    deps = [
        ":string_utils",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file_test",
    size = "small",
    timeout = "short",
    srcs = [
        "file_test.cc",
    ],
    tags = ["nozapfhahn"],
    deps = [
        ":windows",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "device_paths_test",
    size = "small",
    timeout = "short",
    srcs = [
        "device_paths_test.cc",
    ],
    deps = [
        ":device_paths",
        "//internal/base:file_path",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "network_info_test",
    size = "small",
    timeout = "short",
    srcs = [
        "network_info_test.cc",
    ],
    deps = [
        ":network_info",
        "//sharing/internal/impl/windows:platform_windows_libs",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "socket_address_test",
    size = "small",
    timeout = "short",
    srcs = [
        "socket_address_test.cc",
    ],
    deps = [
        ":socket_address",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "scoped_wlan_memory_test",
    size = "small",
    timeout = "short",
    srcs = [
        "scoped_wlan_memory_test.cc",
    ],
    deps = [
        ":scoped_wlan_memory",
        "//sharing/internal/impl/windows:platform_windows_libs",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wlan_client_test",
    size = "small",
    timeout = "short",
    srcs = [
        "wlan_client_test.cc",
    ],
    deps = [
        ":wlan_client",
        "//sharing/internal/impl/windows:platform_windows_libs",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_client_socket_test",
    size = "small",
    timeout = "short",
    srcs = [
        "nearby_client_socket_test.cc",
    ],
    deps = [
        ":socket_address",
        ":windows",
        "//internal/platform:base",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_server_socket_test",
    size = "small",
    timeout = "short",
    srcs = [
        "nearby_server_socket_test.cc",
    ],
    deps = [
        ":socket_address",
        ":windows",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
