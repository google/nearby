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

load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
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
    name = "utils",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    defines = [
        "_SILENCE_CLANG_COROUTINE_MESSAGE",
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":string_utils",
        "//internal/platform:logging",
        "//internal/platform:uuid",
        "//third_party/WinRT:winrt",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "device_info.cc",
        "executor.cc",
        "scheduled_executor.cc",
        "session_manager.cc",
        "submittable_executor.cc",
        "system_clock.cc",
        "task_scheduler.cc",
        "thread_pool.cc",
        "timer.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
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
    ],
    defines = [
        "_SILENCE_CLANG_COROUTINE_MESSAGE",
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
    ],
    linkopts = [
        "powrprof.lib",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":device_paths",
        ":string_utils",
        ":utils",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:types",
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
    defines = [
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
        "WINVER=_WIN32_WINNT_WIN10",
    ],
    tags = ["windows"],
    visibility = ["//:__subpackages__"],
    deps = [
        ":socket_address",
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
    visibility = ["//location/nearby:__subpackages__"],
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
    visibility = ["//location/nearby:__subpackages__"],
    deps = [
        ":scoped_wlan_memory",
        "//internal/platform:logging",
    ],
)

cc_library(
    name = "registry_accessor",
    hdrs = ["registry_accessor.h"],
    tags = ["windows"],
)

cc_library(
    name = "registry",
    srcs = [
        "registry.cc",
    ],
    hdrs = [
        "registry.h",
    ],
    linkopts = [
        "shlwapi.lib",
    ],
    tags = ["windows"],
    visibility = [
        "//internal/platform/implementation/windows:__subpackages__",
        "//location/nearby:__subpackages__",
        "//sharing/internal/impl/windows:__subpackages__",
    ],
    deps = [
        ":registry_accessor",
        "//internal/platform:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mock_registry_accessor",
    testonly = True,
    hdrs = ["mock_registry_accessor.h"],
    tags = ["windows"],
    visibility = ["//visibility:public"],
    deps = [
        ":registry_accessor",
    ],
)

cc_library(
    name = "webrtc_platform",
    srcs = ["webrtc_platform.cc"],
    tags = ["windows"],
    deps = [
        "//connections/implementation/mediums/webrtc:webrtc_medium_impl",
        "//internal/platform:logging",
        "//internal/platform/implementation:webrtc_platform",
    ],
    alwayslink = True,
)

cc_library(
    name = "bluetooth_adapter",
    srcs = [
        "bluetooth_adapter.cc",
    ],
    hdrs = [
        "bluetooth_adapter.h",
    ],
    defines = [
        "_SILENCE_CLANG_COROUTINE_MESSAGE",
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform/implementation:comm",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "ble",
    srcs = [
        "ble.cc",
        "ble_gatt_client.cc",
        "ble_gatt_server.cc",
        "ble_server_socket.cc",
        "ble_socket.cc",
    ],
    hdrs = [
        "ble.h",
        "ble_gatt_client.h",
        "ble_gatt_server.h",
        "ble_server_socket.h",
        "ble_socket.h",
    ],
    copts = ["-D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10"],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":bluetooth_adapter",
        ":utils",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/mediums/ble:ble_advertisement_header",
        "//connections/implementation/mediums/ble:bloom_filter",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:uuid",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "bluetooth_classic",
    srcs = [
        "bluetooth_classic_device.cc",
        "bluetooth_classic_medium.cc",
        "bluetooth_classic_server_socket.cc",
        "bluetooth_classic_socket.cc",
        "bluetooth_pairing.cc",
    ],
    hdrs = [
        "bluetooth_classic.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
        "bluetooth_pairing.h",
    ],
    copts = ["-D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10"],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":bluetooth_adapter",
        ":utils",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "get_custom_save_path",
    srcs = [
        "get_custom_save_path.cc",
    ],
    hdrs = [
        "get_custom_save_path.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":string_utils",
        ":utils",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:logging",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "http_loader",
    srcs = [
        "http_loader.cc",
    ],
    hdrs = [
        "http_loader.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:logging",
        "//internal/platform/implementation:comm",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "input_file",
    srcs = [
        "input_file.cc",
    ],
    hdrs = [
        "input_file.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":string_utils",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "output_file",
    srcs = [
        "output_file.cc",
    ],
    hdrs = [
        "output_file.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "nearby_client_socket",
    srcs = [
        "nearby_client_socket.cc",
    ],
    hdrs = [
        "nearby_client_socket.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":socket_address",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/flags:platform_flags",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "nearby_server_socket",
    srcs = [
        "nearby_server_socket.cc",
    ],
    hdrs = [
        "nearby_server_socket.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":nearby_client_socket",
        ":socket_address",
        "//internal/platform:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "preferences_repository",
    srcs = [
        "preferences_repository.cc",
    ],
    hdrs = [
        "preferences_repository.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "preferences_manager",
    srcs = [
        "preferences_manager.cc",
    ],
    hdrs = [
        "preferences_manager.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":preferences_repository",
        "//internal/base:file_path",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//json",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "wifi_medium",
    srcs = [
        "wifi_medium.cc",
    ],
    hdrs = [
        "wifi.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":scoped_wlan_memory",
        ":wlan_client",
        "//internal/platform:logging",
        "//internal/platform/implementation:comm",
    ],
)

cc_library(
    name = "wifi_intel",
    srcs = [
        "wifi_intel.cc",
    ],
    hdrs = [
        "wifi_intel.h",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:logging",
        "//third_party/intel/pie",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "wifi_direct",
    srcs = [
        "wifi_direct_medium.cc",
        "wifi_direct_server_socket.cc",
        "wifi_direct_socket.cc",
    ],
    hdrs = [
        "wifi_direct.h",
    ],
    copts = ["-D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10"],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":nearby_client_socket",
        ":nearby_server_socket",
        ":network_info",
        ":socket_address",
        ":types",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "wifi_hotspot",
    srcs = [
        "wifi_hotspot_medium.cc",
        "wifi_hotspot_native.cc",
        "wifi_hotspot_server_socket.cc",
        "wifi_hotspot_socket.cc",
    ],
    hdrs = [
        "wifi_hotspot.h",
        "wifi_hotspot_native.h",
        "wifi_hotspot_server_socket.h",
        "wifi_hotspot_socket.h",
    ],
    copts = ["-D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10"],
    linkopts = [
        "iphlpapi.lib",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":nearby_client_socket",
        ":nearby_server_socket",
        ":network_info",
        ":socket_address",
        ":string_utils",
        ":types",
        ":utils",
        ":wifi_intel",
        ":wlan_client",
        "//internal/base:masker",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation:wifi_utils",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "wifi_lan",
    srcs = [
        "wifi_lan_mdns.cc",
        "wifi_lan_medium.cc",
        "wifi_lan_server_socket.cc",
        "wifi_lan_socket.cc",
    ],
    hdrs = [
        "wifi_lan.h",
        "wifi_lan_mdns.h",
    ],
    copts = ["-D_WIN32_WINNT=_WIN32_WINNT_WIN10 -DWINVER=_WIN32_WINNT_WIN10"],
    linkopts = [
        "iphlpapi.lib",
    ],
    tags = ["windows"],
    visibility = ["//visibility:private"],
    deps = [
        ":nearby_client_socket",
        ":nearby_server_socket",
        ":network_info",
        ":registry",
        ":socket_address",
        ":string_utils",
        ":types",
        ":utils",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "windows",
    srcs = [
        "platform.cc",
    ],
    copts = ["-DNO_INTEL_PIE"],
    tags = ["windows"],
    visibility = [
        "//chrome/chromeos/assistant/data_migration/lib:__pkg__",
        "//connections:__subpackages__",
        "//connections:partners",
        "//internal/platform:__subpackages__",
        "//location/nearby:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":ble",
        ":bluetooth_adapter",
        ":bluetooth_classic",
        ":crypto",  # build_cleaner: keep
        ":device_paths",
        ":get_custom_save_path",
        ":http_loader",
        ":input_file",
        ":output_file",
        ":preferences_manager",
        ":string_utils",
        ":types",
        ":wifi_direct",
        ":wifi_hotspot",
        ":wifi_lan",
        ":wifi_medium",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation:wifi_utils",
        "//internal/platform/implementation/shared:count_down_latch",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
    ],
    alwayslink = True,
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
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "test_data",
    testonly = True,
    hdrs = ["test_data.h"],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/platform:base",
        "@nlohmann_json//:json",
    ],
)

proto_library(
    name = "preferences_manager_test_proto",
    testonly = True,
    srcs = ["preferences_manager_test.proto"],
    compatible_with = ["//buildenv/target:non_prod"],
)

cc_proto_library(
    name = "preferences_manager_test_cc_proto",
    testonly = True,
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [":preferences_manager_test_proto"],
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
        "http_loader_test.cc",
        "preferences_manager_test.cc",
        "preferences_repository_test.cc",
        "scheduled_executor_test.cc",
        "submittable_executor_test.cc",
        "task_scheduler_test.cc",
        "thread_pool_test.cc",
        "timer_test.cc",
        "utils_test.cc",
        "wifi_direct_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_medium_test.cc",
    ],
    tags = ["nozapfhahn"],
    deps = [
        ":ble",
        ":bluetooth_adapter",
        ":crypto",
        ":http_loader",
        ":preferences_manager",
        ":preferences_manager_test_cc_proto",
        ":preferences_repository",
        ":string_utils",
        ":test_data",
        ":types",
        ":utils",
        ":wifi_direct",
        ":wifi_hotspot",
        ":wifi_medium",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//net/proto2/contrib/parse_proto:parse_text_proto",
        "//third_party/WinRT:winrt",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "get_custom_save_path_test",
    size = "small",
    timeout = "short",
    srcs = [
        "get_custom_save_path_test.cc",
    ],
    tags = ["windows"],
    deps = [
        ":get_custom_save_path",
        "//internal/base:file_path",
        "//internal/base:files",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "platform_test",
    size = "small",
    timeout = "short",
    srcs = [
        "platform_test.cc",
    ],
    tags = ["windows"],
    deps = [
        ":device_paths",
        ":get_custom_save_path",
        ":windows",
        "//internal/base:file_path",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
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
    name = "input_file_test",
    size = "small",
    timeout = "short",
    srcs = [
        "input_file_test.cc",
    ],
    tags = ["windows"],
    deps = [
        ":input_file",
        ":output_file",
        ":string_utils",
        "//internal/platform:base",
        "//internal/platform:logging",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "output_file_test",
    size = "small",
    timeout = "short",
    srcs = [
        "output_file_test.cc",
    ],
    tags = ["windows"],
    deps = [
        ":input_file",
        ":output_file",
        ":string_utils",
        "//internal/platform:base",
        "//internal/platform:logging",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
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
        ":nearby_client_socket",
        ":socket_address",
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
        ":nearby_server_socket",
        ":socket_address",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "registry_test",
    size = "small",
    timeout = "short",
    srcs = [
        "registry_test.cc",
    ],
    deps = [
        ":mock_registry_accessor",
        ":registry",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
