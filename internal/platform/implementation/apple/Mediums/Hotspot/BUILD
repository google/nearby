# Copyright 2025 Google LLC
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

load("@rules_cc//cc:objc_library.bzl", "objc_library")

licenses(["notice"])

package(default_visibility = [
    "//internal/platform/implementation/apple:__subpackages__",
])

objc_library(
    name = "Hotspot",
    srcs = [
        "GNCHotspotMedium.m",
        "GNCHotspotSocket.m",
    ],
    hdrs = [
        "GNCHotspotMedium.h",
        "GNCHotspotSocket.h",
    ],
    deps = [
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//third_party/apple_frameworks:CoreLocation",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/apple_frameworks:NetworkExtension",
        "//third_party/apple_frameworks:SystemConfiguration",
    ],
)
