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
    name = "internal",
    srcs = [
        "base_bwu_handler.cc",
        "base_endpoint_channel.cc",
        "base_pcp_handler.cc",
        "ble_advertisement.cc",
        "ble_endpoint_channel.cc",
        "ble_v2_endpoint_channel.cc",
        "bluetooth_bwu_handler.cc",
        "bluetooth_device_name.cc",
        "bluetooth_endpoint_channel.cc",
        "bwu_manager.cc",
        "client_proxy.cc",
        "connections_authentication_transport.cc",
        "encryption_runner.cc",
        "endpoint_channel_manager.cc",
        "endpoint_manager.cc",
        "injected_bluetooth_device_store.cc",
        "internal_payload.cc",
        "internal_payload_factory.cc",
        "offline_frames.cc",
        "offline_frames_validator.cc",
        "offline_service_controller.cc",
        "p2p_cluster_pcp_handler.cc",
        "p2p_point_to_point_pcp_handler.cc",
        "p2p_star_pcp_handler.cc",
        "payload_manager.cc",
        "pcp_manager.cc",
        "reconnect_manager.cc",
        "service_controller_router.cc",
        "webrtc_bwu_handler.cc",
        "webrtc_bwu_handler_stub.cc",
        "webrtc_endpoint_channel.cc",
        "wifi_direct_bwu_handler.cc",
        "wifi_direct_endpoint_channel.cc",
        "wifi_hotspot_bwu_handler.cc",
        "wifi_hotspot_endpoint_channel.cc",
        "wifi_lan_bwu_handler.cc",
        "wifi_lan_endpoint_channel.cc",
        "wifi_lan_service_info.cc",
    ],
    hdrs = [
        "base_bwu_handler.h",
        "base_endpoint_channel.h",
        "base_pcp_handler.h",
        "ble_advertisement.h",
        "ble_endpoint_channel.h",
        "ble_v2_endpoint_channel.h",
        "bluetooth_bwu_handler.h",
        "bluetooth_device_name.h",
        "bluetooth_endpoint_channel.h",
        "bwu_handler.h",
        "bwu_manager.h",
        "client_proxy.h",
        "connections_authentication_transport.h",
        "encryption_runner.h",
        "endpoint_channel.h",
        "endpoint_channel_manager.h",
        "endpoint_manager.h",
        "injected_bluetooth_device_store.h",
        "internal_payload.h",
        "internal_payload_factory.h",
        "offline_frames.h",
        "offline_frames_validator.h",
        "offline_service_controller.h",
        "p2p_cluster_pcp_handler.h",
        "p2p_point_to_point_pcp_handler.h",
        "p2p_star_pcp_handler.h",
        "payload_manager.h",
        "pcp.h",
        "pcp_handler.h",
        "pcp_manager.h",
        "reconnect_manager.h",
        "service_controller.h",
        "service_controller_router.h",
        "service_id_constants.h",
        "webrtc_bwu_handler.h",
        "webrtc_bwu_handler_stub.h",
        "webrtc_endpoint_channel.h",
        "wifi_direct_bwu_handler.h",
        "wifi_direct_endpoint_channel.h",
        "wifi_hotspot_bwu_handler.h",
        "wifi_hotspot_endpoint_channel.h",
        "wifi_lan_bwu_handler.h",
        "wifi_lan_endpoint_channel.h",
        "wifi_lan_service_info.h",
    ],
    copts = [
        "-DCORE_ADAPTER_DLL",
        "-DNO_WEBRTC",
    ],
    visibility = [
        "//chrome/chromeos/assistant/data_migration/lib:__pkg__",
        "//connections:__pkg__",
        "//connections/implementation/fuzzers:__pkg__",
        "//connections/implementation/mediums/multiplex:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//connections/v3:v3_types",
        "//internal/analytics:event_logger",
        "//internal/flags:nearby_flags",
        "//internal/interop:authentication_status",
        "//internal/interop:authentication_transport_interface",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:connection_info",
        "//internal/platform:error_code_recorder",
        "//internal/platform:types",
        "//internal/platform:util",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation:wifi_utils",
        "//internal/proto/analytics:connections_log_cc_proto",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_ukey2//:ukey2",
    ],
)

cc_library(
    name = "message_lite",
    hdrs = [
        "message_lite.h",
    ],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        "@com_google_protobuf//:protobuf_lite",
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
        "mock_service_controller.h",
        "mock_service_controller_router.h",
        "offline_simulation_user.h",
        "simulation_user.h",
    ],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        ":internal",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "bwu_test",
    srcs = [
        "base_bwu_handler_test.cc",
        "bluetooth_bwu_test.cc",
        "bwu_manager_test.cc",
        "wifi_direct_bwu_test.cc",
        "wifi_hotspot_bwu_test.cc",
    ],
    deps = [
        ":internal",
        ":internal_test",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/proto/analytics:connections_log_cc_proto",
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pcp_manager_test",
    srcs = ["pcp_manager_test.cc"],
    deps = [
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
    ],
    shard_count = 8,
    deps = [
        ":internal",
        ":internal_test",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:authentication_status",
        "//internal/interop:authentication_transport_interface",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
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
        ":internal",
        "//internal/platform:base",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "offline_frames_test",
    srcs = ["offline_frames_validator_test.cc"],
    deps = [
        ":internal",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
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
        ":internal",
        "//base:casts",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/v3:v3_types",
        "//internal/analytics:mock_event_logger",
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
        ":internal",
        "//connections/implementation/analytics",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
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
        ":internal",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test",
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
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
        ":internal",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/proto/analytics:connections_log_cc_proto",
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
        "//connections/implementation/analytics",
        "//internal/platform:base",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
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
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "reconnect_manager_test",
    srcs = [
        "reconnect_manager_test.cc",
    ],
    deps = [
        ":internal",
        ":internal_test",
        "//connections:core_types",
        "//connections/implementation/mediums",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
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
        ":internal",
        ":internal_test",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//connections/v3:v3_types",
        "//internal/flags:nearby_flags",
        "//internal/interop:authentication_status",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
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
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
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
        "//connections:core_types",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
