load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

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

cmake(
    name = "ukey2",
    env = {
        "CC": "clang",
        "CXX": "clang++",
    },
    generate_args = [
        "-DBYPASS_TESTING=ON",  # Set the flags in CMakeLists.txt (ukey2 & its dependencies) here.
        "-Dukey2_USE_LOCAL_ABSL=ON",
        "-Dukey2_USE_LOCAL_PROTOBUF=ON",  # Add -DCMAKE_BUILD_TYPE=Debug for debugger & Valgrind
    ],
    lib_source = "@com_google_ukey2//:all_srcs",
    out_static_libs = [
        "libproto_device_to_device_messages_cc_proto.a",
        "libproto_securegcm_cc_proto.a",
        "libproto_securemessage_cc_proto.a",
        "libproto_ukey_cc_proto.a",
        "libsecuremessage.a",
        "libukey2.a",
    ],
)

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
        "service_controller_router.cc",
        "webrtc_endpoint_channel.cc",
        "wifi_direct_bwu_handler.cc",
        "wifi_direct_endpoint_channel.cc",
        "wifi_hotspot_bwu_handler.cc",
        "wifi_hotspot_endpoint_channel.cc",
        "wifi_lan_bwu_handler.cc",
        "wifi_lan_endpoint_channel.cc",
        "wifi_lan_service_info.cc",
    ] + select({
        "@platforms//cpu:arm": [
            "webrtc_bwu_handler_stub.cc",
        ],
        "//conditions:default": [
            "webrtc_bwu_handler_stub.cc",
        ],
    }),
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
        "service_controller.h",
        "service_controller_router.h",
        "service_id_constants.h",
        "webrtc_endpoint_channel.h",
        "wifi_direct_bwu_handler.h",
        "wifi_direct_endpoint_channel.h",
        "wifi_hotspot_bwu_handler.h",
        "wifi_hotspot_endpoint_channel.h",
        "wifi_lan_bwu_handler.h",
        "wifi_lan_endpoint_channel.h",
        "wifi_lan_service_info.h",
    ] + select({
        "@platforms//cpu:arm": [
            "webrtc_bwu_handler_stub.h",
        ],
        "//conditions:default": [
            "webrtc_bwu_handler_stub.h",
        ],
    }),
    copts = ["-DCORE_ADAPTER_DLL"] + select({
        "@platforms//cpu:arm": [
            "-DNO_WEBRTC",
        ],
        "//conditions:default": [],
    }),
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__pkg__",
        "//connections/implementation/fuzzers:__pkg__",
    ],
    deps = [
        ":message_lite",
        ":ukey2",
        "//connections:core_types",
        "//connections/implementation/analytics",
        "//connections/implementation/mediums",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal:device",
        "//internal/analytics:event_logger",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:error_code_recorder",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform:util",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation/shared:file",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "message_lite",
    hdrs = [
        "message_lite.h",
    ],
    defines = ["NO_WEBRTC"],
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
        "mock_service_controller.h",
        "mock_service_controller_router.h",
        "offline_simulation_user.h",
        "simulation_user.h",
    ],
    defines = ["NO_WEBRTC"],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        ":internal",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "core_internal_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "base_bwu_handler_test.cc",
        "base_endpoint_channel_test.cc",
        "base_pcp_handler_test.cc",
        "ble_advertisement_test.cc",
        "bluetooth_device_name_test.cc",
        "bwu_manager_test.cc",
        "client_proxy_test.cc",
        "encryption_runner_test.cc",
        "endpoint_channel_manager_test.cc",
        "endpoint_manager_test.cc",
        "injected_bluetooth_device_store_test.cc",
        "internal_payload_factory_test.cc",
        "offline_frames_validator_test.cc",
        "offline_service_controller_test.cc",
        "p2p_cluster_pcp_handler_test.cc",
        "p2p_point_to_point_pcp_handler_test.cc",
        "payload_manager_test.cc",
        "pcp_manager_test.cc",
        "service_controller_router_test.cc",
        "wifi_direct_bwu_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_lan_service_info_test.cc",
    ],
    defines = ["NO_WEBRTC"],
    shard_count = 16,
    deps = [
        ":internal",
        ":internal_test",
        ":ukey2",
        "//connections:core_types",
        "//connections/implementation/mediums",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/analytics:event_logger",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
