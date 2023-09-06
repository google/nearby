# Copyright 2020-2023 Google LLC
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

load("//tools/build_defs/swift:swift_explicit_module_build_test.bzl", "swift_explicit_module_build_test")
load("//third_party/nearby:minimum_os.bzl", "IOS_MINIMUM_OS")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

objc_library(
    name = "Central",
    srcs = glob([
        "Source/Central/*.m",
    ]),
    hdrs = glob([
        "Source/Central/*.h",
    ]) + [
        "Source/GNSCentral.h",
    ],
    deps = [
        ":Shared",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ],
)

objc_library(
    name = "Peripheral",
    srcs = glob([
        "Source/Peripheral/*.m",
    ]),
    hdrs = glob([
        "Source/Peripheral/*.h",
    ]) + [
        "Source/GNSPeripheral.h",
    ],
    deps = [
        ":Shared",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:CoreFoundation",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:QuartzCore",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ] + select({
        "@platforms//os:platform_ios": [
            "//third_party/apple_frameworks:UIKit",
        ],
        "//conditions:default": [],
    }),
)

objc_library(
    name = "Shared",
    srcs = glob([
        "Source/Shared/*.m",
    ]),
    hdrs = glob([
        "Source/Shared/*.h",
    ]) + [
        "Source/GNSShared.h",
    ],
    deps = [
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:CoreFoundation",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:QuartzCore",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ],
)

swift_explicit_module_build_test(
    name = "swift_explicit_module_build_test",
    ignore_headerless_targets = True,
    minimum_os_version = IOS_MINIMUM_OS,
    platform_type = "ios",
)
