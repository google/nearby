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

load("//tools/build_defs/apple:ios.bzl", "ios_unit_test")
load("//tools/build_defs/swift:swift_library.bzl", "swift_library")

licenses(["notice"])

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
    deprecation = "Please use //third_party/nearby/connections/swift/NearbyConnections",
    features = ["-layering_check"],
    module_name = "NearbyConnections",
    visibility = [
        "//googlemac/iPhone/Nearby/HelloConnections:__pkg__",
        "//googlemac/iPhone/Nearby/Sharing/Source/UI/HomeViewController:__pkg__",
    ],
    deps = [
        "//connections:core",
        "//connections:core_types",
        "//internal/platform/implementation/apple",
        "//third_party/apple_frameworks:Foundation",
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
    minimum_os_version = "13.0",
    runner = "//testing/utp/ios:IOS_13",
    deps = [
        ":BuildTestsLib",
    ],
)
