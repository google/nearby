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
load("//tools/build_defs/apple:ios.bzl", "ios_static_framework")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

objc_library(
    name = "Connections",
    srcs = [
        "Source/Internal/GNCAdvertiser.mm",
        "Source/Internal/GNCCore.mm",
        "Source/Internal/GNCCoreConnection.mm",
        "Source/Internal/GNCDiscoverer.mm",
        "Source/Internal/GNCPayload.m",
        "Source/Internal/GNCPayloadListener.mm",
        "Source/Internal/GNCUtils.mm",
    ],
    hdrs = [
        "Source/GNCAdvertiser.h",
        "Source/GNCConnection.h",
        "Source/GNCConnections.h",
        "Source/GNCDiscoverer.h",
        "Source/GNCPayload.h",
        "Source/Internal/GNCCore.h",
        "Source/Internal/GNCCoreConnection.h",
        "Source/Internal/GNCPayload+Internal.h",
        "Source/Internal/GNCPayloadListener.h",
        "Source/Internal/GNCUtils.h",
    ],
    deps = [
        "//absl/functional:bind_front",
        "//core",
        "//core:core_types",
        "//platform/impl/ios/Source/Mediums",
        "//platform/impl/ios/Source/Platform",
        "//platform/impl/ios/Source/Shared",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ],
)

MIN_IOS_VERSION = "12.0"

HDRS_POD = [
    "Source/GNCAdvertiser.h",
    "Source/GNCConnection.h",
    "Source/GNCDiscoverer.h",
    "Source/GNCPayload.h",
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
