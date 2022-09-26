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

# TODO(b/240046236): Rename this package as `objc`.

load("//tools/build_defs/apple:ios.bzl", "ios_static_framework", "ios_unit_test")
load("//tools/build_defs/swift:swift_library.bzl", "swift_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

objc_library(
    name = "Connections",
    srcs = [
        "Internal/GNCAdvertiser.mm",
        "Internal/GNCCore.h",
        "Internal/GNCCore.mm",
        "Internal/GNCCoreConnection.h",
        "Internal/GNCCoreConnection.mm",
        "Internal/GNCDiscoverer.mm",
        "Internal/GNCPayload.mm",
        "Internal/GNCPayload+Internal.h",
        "Internal/GNCPayloadListener.h",
        "Internal/GNCPayloadListener.mm",
        "Internal/GNCUtils.h",
        "Internal/GNCUtils.mm",
    ],
    hdrs = [
        "Public/NearbyConnections/GNCAdvertiser.h",
        "Public/NearbyConnections/GNCConnection.h",
        "Public/NearbyConnections/GNCConnections.h",
        "Public/NearbyConnections/GNCDiscoverer.h",
        "Public/NearbyConnections/GNCPayload.h",
    ],
    features = ["-layering_check"],
    module_name = "NearbyConnections",
    deps = [
        "//third_party/apple_frameworks:Foundation",
        "//connections:core",
        "//connections:core_types",
        "//internal/platform/implementation/ios:Platform",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

swift_library(
    name = "BuildTestsLib",
    testonly = 1,
    srcs = ["BuildTests/Test.swift"],
    deps = [
        ":Connections",
        "//third_party/apple_frameworks:XCTest",
    ],
)

ios_unit_test(
    name = "BuildTests",
    minimum_os_version = "12.0",
    runner = "//testing/utp/ios:IOS_12",
    deps = [
        ":BuildTestsLib",
    ],
)

MIN_IOS_VERSION = "12.0"

HDRS_POD = [
    "GNCAdvertiser.h",
    "GNCConnection.h",
    "GNCDiscoverer.h",
    "GNCPayload.h",
]

ios_static_framework(
    name = "NearbyConnections_framework",
    hdrs = HDRS_POD,
    bundle_name = "NearbyConnections",
    minimum_os_version = MIN_IOS_VERSION,
    deps = [
        ":Connections",
    ],
)
