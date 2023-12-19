# Copyright 2021 Google LLC
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
    textual_hdrs = [
        "app_info.h",
        "bluetooth_adapter.h",
        "fast_init_ble_beacon.h",
        "fast_initiation_manager.h",
        "network_monitor.h",
        "preference_manager.h",
        "private_certificate_data.h",
        "sharing_platform.h",
        "shell.h",
        "system_info.h",
        "wifi_adapter.h",
    ],
    visibility = [
        "//location/nearby/analytics/cpp/logging:__pkg__",
        "//location/nearby/apps/better_together/macos/nearby_share:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        "//internal/platform:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "platform_factory",
    srcs = select({
        "@platforms//os:osx": [
            "platform_factory_macos.cc",
        ],
        "@platforms//os:windows": [
            "platform_factory_windows.cc",
        ],
        "//conditions:default": [
            "platform_factory_g3.cc",
        ],
    }),
    hdrs = ["platform_factory.h"],
    visibility = [
        "//location/nearby/apps/better_together:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        ":platform",
        "@com_google_absl//absl/base:no_destructor",
    ] + select({
        "@platforms//os:osx": [
            "//third_party/nearby/sharing/internal/impl/apple",
        ],
        "@platforms//os:windows": [
            "//third_party/nearby/sharing/internal/impl/windows:platform_windows",
        ],
        "//conditions:default": [
            "//third_party/nearby/sharing/internal/impl/g3:platform_g3",
        ],
    }),
)

cc_library(
    name = "mock_sharing_platform",
    testonly = True,
    hdrs = [
        "mock_app_info.h",
        "mock_bluetooth_adapter.h",
        "mock_network_monitor.h",
        "mock_sharing_platform.h",
        "mock_system_info.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":platform",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)
