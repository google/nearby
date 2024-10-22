# Copyright 2023 Google LLC
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
    name = "platform",
    hdrs = [
        "app_info.h",
        "bluetooth_adapter.h",
        "fast_init_ble_beacon.h",
        "fast_initiation_manager.h",
        "network_monitor.h",
        "preference_manager.h",
        "private_certificate_data.h",
        "public_certificate_database.h",
        "sharing_platform.h",
        "sharing_rpc_client.h",
        "sharing_rpc_notifier.h",
        "system_info.h",
        "wifi_adapter.h",
    ],
    visibility = [
        "//location/nearby/analytics/cpp/logging:__pkg__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/sharing/sdk/test_client:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "//internal/platform:types",
        "//internal/platform/implementation:account_manager",
        "//sharing/analytics",
        "//sharing/proto:share_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "mock_sharing_platform",
    testonly = True,
    srcs = [
        "fake_nearby_share_client.cc",
    ],
    hdrs = [
        "fake_nearby_share_client.h",
        "mock_app_info.h",
        "mock_bluetooth_adapter.h",
        "mock_fast_init_ble_beacon.h",
        "mock_fast_initiation_manager.h",
        "mock_network_monitor.h",
        "mock_public_certificate_db.h",
        "mock_sharing_platform.h",
        "mock_system_info.h",
        "mock_wifi_adapter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":platform",
        "//internal/platform:types",
        "//internal/platform/implementation:account_manager",
        "//sharing/analytics",
        "//sharing/proto:share_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)
