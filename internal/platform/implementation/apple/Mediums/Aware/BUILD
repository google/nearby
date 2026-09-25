# Copyright 2026 Google LLC
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

load("//tools/build_defs/swift:swift_library.bzl", "swift_library")

licenses(["notice"])

package(default_visibility = [
    "//internal/platform/implementation/apple:__subpackages__",
])

swift_library(
    name = "WiFiAwareMedium",
    srcs = ["WiFiAwareMedium.swift"],
    generates_header = True,
    target_compatible_with = ["//third_party/bazel_platforms/os:ios"],
    deps = [
        "//third_party/apple_frameworks:DeviceDiscoveryUI",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/apple_frameworks:WiFiAware",
        "//third_party/apple_frameworks:os",
    ] + select({
        "@platforms//os:platform_ios": [
            "//third_party/apple_frameworks:UIKit",
        ],
        "//conditions:default": [],
    }),
)

alias(
    name = "Aware",
    actual = ":WiFiAwareMedium",
)
