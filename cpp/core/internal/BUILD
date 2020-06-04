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

cc_library(
    name = "internal",
    srcs = [
        "bandwidth_upgrade_manager.cc",
        "base_bandwidth_upgrade_handler.cc",
        "base_endpoint_channel.cc",
        "ble_advertisement.cc",
        "ble_endpoint_channel.cc",
        "bluetooth_device_name.cc",
        "bluetooth_endpoint_channel.cc",
        "endpoint_channel_manager.cc",
        "internal_payload.cc",
        "internal_payload.h",
        "loop_runner.cc",
        "loop_runner.h",
        "offline_frames.cc",
        "wifi_lan_endpoint_channel.cc",
        "wifi_lan_service_info.cc",
    ],
    hdrs = [
        "bandwidth_upgrade_handler.h",
        "bandwidth_upgrade_manager.h",
        "base_bandwidth_upgrade_handler.h",
        "base_endpoint_channel.h",
        "base_pcp_handler.cc",
        "base_pcp_handler.h",
        "ble_advertisement.h",
        "ble_compat.h",
        "ble_endpoint_channel.h",
        "bluetooth_device_name.h",
        "bluetooth_endpoint_channel.h",
        "client_proxy.cc",
        "client_proxy.h",
        "encryption_runner.cc",
        "encryption_runner.h",
        "endpoint_channel.h",
        "endpoint_channel_manager.h",
        "endpoint_manager.cc",
        "endpoint_manager.h",
        "internal_payload_factory.cc",
        "internal_payload_factory.h",
        "medium_manager.cc",
        "medium_manager.h",
        "offline_frames.h",
        "offline_service_controller.cc",
        "offline_service_controller.h",
        "p2p_cluster_pcp_handler.cc",
        "p2p_cluster_pcp_handler.h",
        "p2p_point_to_point_pcp_handler.cc",
        "p2p_point_to_point_pcp_handler.h",
        "p2p_star_pcp_handler.cc",
        "p2p_star_pcp_handler.h",
        "payload_manager.cc",
        "payload_manager.h",
        "pcp.h",
        "pcp_handler.h",
        "pcp_manager.cc",
        "pcp_manager.h",
        "service_controller.h",
        "service_controller_router.cc",
        "service_controller_router.h",
        "wifi_lan_endpoint_channel.h",
        "wifi_lan_service_info.h",
        "wifi_lan_upgrade_handler.cc",
        "wifi_lan_upgrade_handler.h",
    ],
    visibility = [
        "//core:__pkg__",
    ],
    deps = [
        "//core:types",
        "//core/internal/mediums",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform:logging",
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
        "//proto:connections_enums_portable_proto",
        "//net/proto2/compat/public:proto2_lite",
        "//securegcm:ukey2",
        "//absl/strings",
    ],
)

# TODO(apolyudov): remove when api v2 rework is done.
cc_library(
    name = "message_lite",
    hdrs = [
        "message_lite.h",
    ],
    visibility = [
        "//core:__subpackages__",
        "//core_v2:__subpackages__",
    ],
    deps = [
        "//net/proto2/compat/public:proto2_lite",
    ],
)

cc_test(
    name = "base_endpoint_channel_test",
    srcs = ["base_endpoint_channel_test.cc"],
    deps = [
        ":internal",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/g3",
        "//proto:connections_enums_portable_proto",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "bluetooth_device_name_test",
    srcs = ["bluetooth_device_name_test.cc"],
    deps = [
        ":internal",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/g3",
        "//platform/port:string",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "ble_advertisement_test",
    srcs = ["ble_advertisement_test.cc"],
    deps = [
        ":internal",
        "//platform/api",
        "//platform/impl/g3",
        "//platform/port:string",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "wifi_lan_service_info_test",
    srcs = ["wifi_lan_service_info_test.cc"],
    deps = [
        ":internal",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/g3",
        "//platform/port:string",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "offline_frames_test",
    srcs = [
        "offline_frames_test.cc",
    ],
    deps = [
        ":internal",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform:types",
        "//platform/api",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
    ],
)
