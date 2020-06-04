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
        "base_endpoint_channel.cc",
        "base_pcp_handler.cc",
        "ble_advertisement.cc",
        "client_proxy.cc",
        "encryption_runner.cc",
        "endpoint_channel_manager.cc",
        "endpoint_manager.cc",
        "offline_frames.cc",
        "service_controller_router.cc",
        "wifi_lan_service_info.cc",
    ],
    hdrs = [
        "base_endpoint_channel.h",
        "base_pcp_handler.h",
        "ble_advertisement.h",
        "client_proxy.h",
        "encryption_runner.h",
        "endpoint_channel.h",
        "endpoint_channel_manager.h",
        "endpoint_manager.h",
        "offline_frames.h",
        "pcp.h",
        "pcp_handler.h",
        "service_controller.h",
        "service_controller_router.h",
        "wifi_lan_service_info.h",
    ],
    visibility = [
        "//core_v2:__pkg__",
    ],
    deps = [
        "//core/internal:message_lite",
        "//core_v2:core_types",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform_v2/base",
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//proto:connections_enums_portable_proto",
        "//securegcm:ukey2",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//absl/time",
        "//absl/types:span",
    ],
)

cc_library(
    name = "internal_test",
    testonly = True,
    hdrs = [
        "mock_service_controller.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
    ],
    deps = [
        ":internal",
        "//testing/base/public:gunit",
    ],
)

cc_test(
    name = "core_v2_internal_test",
    size = "small",
    srcs = [
        "base_endpoint_channel_test.cc",
        "base_pcp_handler_test.cc",
        "ble_advertisement_test.cc",
        "client_proxy_test.cc",
        "encryption_runner_test.cc",
        "endpoint_channel_manager_test.cc",
        "endpoint_manager_test.cc",
        "offline_frames_test.cc",
        "service_controller_router_test.cc",
        "wifi_lan_service_info_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":internal",
        ":internal_test",
        "//core_v2:core_types",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform_v2/base",
        "//platform_v2/impl/g3",  # build_cleaner: keep
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//proto:connections_enums_portable_proto",
        "//securegcm:ukey2",
        "//testing/base/public:gunit",
        "//testing/base/public:gunit_main",
        "//absl/container:flat_hash_set",
        "//absl/synchronization",
        "//absl/time",
        "//absl/types:span",
    ],
)
