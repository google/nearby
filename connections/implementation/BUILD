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
    name = "types",
    hdrs = [
        "pcp.h",
        "webrtc_state.h",
    ],
    visibility = ["//connections:__subpackages__"],
)

cc_library(
    name = "ble_advertisement",
    srcs = [
        "ble_advertisement.cc",
    ],
    hdrs = [
        "ble_advertisement.h",
    ],
    visibility = ["//connections:__subpackages__"],
    deps = [
        ":types",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:util",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "service_id_constants",
    hdrs = ["service_id_constants.h"],
    visibility = ["//connections:__subpackages__"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "bwu_handler",
    srcs = ["base_bwu_handler.cc"],
    hdrs = [
        "base_bwu_handler.h",
        "bwu_handler.h",
    ],
    visibility = ["//connections:__subpackages__"],
    deps = [
        ":client_proxy",
        ":endpoint_channel",
        ":service_id_constants",
        "//internal/platform:base",
        "//internal/platform:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_library(
    name = "offline_frames",
    srcs = [
        "offline_frames.cc",
        "offline_frames_validator.cc",
    ],
    hdrs = [
        "internal_payload.h",
        "offline_frames.h",
        "offline_frames_validator.h",
    ],
    visibility = ["//connections:__subpackages__"],
    deps = [
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "client_proxy",
    srcs = ["client_proxy.cc"],
    hdrs = ["client_proxy.h"],
    visibility = ["//connections:__subpackages__"],
    deps = [
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums/advertisements:dct_advertisement",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//connections/v3:v3_types",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/flags:nearby_flags",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:error_code_recorder",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "endpoint_channel",
    srcs = [
        "base_endpoint_channel.cc",
        "endpoint_channel_manager.cc",
    ],
    hdrs = [
        "base_endpoint_channel.h",
        "endpoint_channel.h",
        "endpoint_channel_manager.h",
    ],
    visibility = ["//connections:__subpackages__"],
    deps = [
        ":client_proxy",
        ":offline_frames",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_ukey2//:ukey2",
    ],
)

cc_library(
    name = "internal",
    srcs = [
        "base_pcp_handler.cc",
        "bluetooth_device_name.cc",
        "bwu_manager.cc",
        "connections_authentication_transport.cc",
        "encryption_runner.cc",
        "endpoint_manager.cc",
        "injected_bluetooth_device_store.cc",
        "internal_payload.cc",
        "internal_payload_factory.cc",
        "offline_service_controller.cc",
        "p2p_cluster_pcp_handler.cc",
        "p2p_point_to_point_pcp_handler.cc",
        "p2p_star_pcp_handler.cc",
        "payload_manager.cc",
        "pcp_manager.cc",
        "service_controller_router.cc",
        "wifi_lan_service_info.cc",
    ],
    hdrs = [
        "base_pcp_handler.h",
        "bluetooth_device_name.h",
        "bwu_manager.h",
        "connections_authentication_transport.h",
        "encryption_runner.h",
        "endpoint_manager.h",
        "injected_bluetooth_device_store.h",
        "internal_payload_factory.h",
        "offline_service_controller.h",
        "p2p_cluster_pcp_handler.h",
        "p2p_point_to_point_pcp_handler.h",
        "p2p_star_pcp_handler.h",
        "payload_manager.h",
        "pcp_handler.h",
        "pcp_manager.h",
        "service_controller.h",
        "service_controller_router.h",
        "wifi_lan_service_info.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//chrome/chromeos/assistant/data_migration/lib:__pkg__",
        "//connections:__pkg__",
        "//connections:partners",
        "//connections/implementation/fuzzers:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":ble_advertisement",
        ":bwu_handler",
        ":client_proxy",
        ":endpoint_channel",
        ":offline_frames",
        ":service_id_constants",
        ":types",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/mediums:webrtc",
        "//connections/implementation/mediums:webrtc_peer_id",
        "//connections/implementation/mediums/advertisements:dct_advertisement",
        "//connections/implementation/mediums/advertisements:util",
        "//connections/implementation/mediums/ble:ble_advertisement_header",
        "//connections/implementation/mediums/ble:ble_socket",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:authentication_status",
        "//internal/interop:authentication_transport_interface",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:connection_info",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform:util",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation:wifi_utils",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_ukey2//:ukey2",
    ],
)

cc_library(
    name = "internal_test",
    testonly = True,
    srcs = [
        "offline_simulation_user.cc",
        "simulation_user.cc",
    ],
    hdrs = [
        "fake_bwu_handler.h",
        "fake_endpoint_channel.h",
        "mock_device.h",
        "mock_endpoint_channel.h",
        "mock_service_controller.h",
        "mock_service_controller_router.h",
        "offline_simulation_user.h",
        "simulation_user.h",
    ],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        ":bwu_handler",
        ":client_proxy",
        ":endpoint_channel",
        ":internal",
        ":offline_frames",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "bwu_test",
    srcs = [
        "base_bwu_handler_test.cc",
        "bwu_manager_test.cc",
    ],
    deps = [
        ":bwu_handler",
        ":client_proxy",
        ":endpoint_channel",
        ":internal",
        ":internal_test",
        ":offline_frames",
        ":service_id_constants",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation:platform",  # build_cleaner: keep
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pcp_manager_test",
    srcs = ["pcp_manager_test.cc"],
    deps = [
        ":endpoint_channel",
        ":internal",
        ":internal_test",
        "//connections:core_types",
        "//connections/v3:v3_types",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pcp_handler_test",
    timeout = "moderate",
    srcs = [
        "base_pcp_handler_test.cc",
        "p2p_cluster_pcp_handler_test.cc",
        "p2p_point_to_point_pcp_handler_test.cc",
        "p2p_star_pcp_handler_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":client_proxy",
        ":endpoint_channel",
        ":internal",
        ":internal_test",
        ":offline_frames",
        ":types",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/analytics:mock_analytics_recorder",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//connections/implementation/mediums:webrtc_peer_id",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:authentication_status",
        "//internal/interop:authentication_transport_interface",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "advertisement_test",
    srcs = [
        "ble_advertisement_test.cc",
        "bluetooth_device_name_test.cc",
        "wifi_lan_service_info_test.cc",
    ],
    deps = [
        ":ble_advertisement",
        ":internal",
        ":types",
        "//internal/platform:base",
        "//internal/platform:mac_address",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "offline_frames_test",
    srcs = ["offline_frames_validator_test.cc"],
    deps = [
        ":offline_frames",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:mac_address",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "client_proxy_test",
    srcs = [
        "client_proxy_test.cc",
    ],
    deps = [
        ":client_proxy",
        "//connections:core_types",
        "//connections/implementation/analytics:mock_analytics_recorder",
        "//connections/implementation/flags:connections_flags",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encryption_runner_test",
    srcs = [
        "encryption_runner_test.cc",
    ],
    deps = [
        ":client_proxy",
        ":endpoint_channel",
        ":internal",
        "//connections/implementation/analytics",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_ukey2//:ukey2",
    ],
)

cc_test(
    name = "endpoint_manager_test",
    srcs = [
        "endpoint_manager_test.cc",
    ],
    deps = [
        ":client_proxy",
        ":endpoint_channel",
        ":internal",
        ":internal_test",
        ":offline_frames",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test",
        "//proto:connections_enums_cc_proto",
        "//testing/fuzzing:fuzztest",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "endpoint_channel_test",
    srcs = [
        "base_endpoint_channel_test.cc",
        "endpoint_channel_manager_test.cc",
    ],
    deps = [
        ":client_proxy",
        ":endpoint_channel",
        ":internal",
        ":offline_frames",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_ukey2//:ukey2",
    ],
)

cc_test(
    name = "connections_authentication_transport_test",
    srcs = [
        "connections_authentication_transport_test.cc",
    ],
    deps = [
        ":internal",
        ":internal_test",
        "//internal/platform:base",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "payload_manager_test",
    srcs = [
        "payload_manager_test.cc",
    ],
    deps = [
        ":internal",
        ":internal_test",
        ":offline_frames",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "service_controller_test",
    srcs = [
        "offline_service_controller_test.cc",
        "service_controller_router_test.cc",
    ],
    deps = [
        ":client_proxy",
        ":internal",
        ":internal_test",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:authentication_status",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "injected_bluetooth_device_store_test",
    srcs = [
        "injected_bluetooth_device_store_test.cc",
    ],
    deps = [
        ":internal",
        ":types",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:mac_address",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "internal_payload_factory_test",
    srcs = [
        "internal_payload_factory_test.cc",
    ],
    deps = [
        ":internal",
        ":offline_frames",
        "//connections:core_types",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
