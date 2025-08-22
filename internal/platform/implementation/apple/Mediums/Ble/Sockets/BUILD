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

load("@rules_apple//apple:ios.bzl", "ios_unit_test")
load("@rules_cc//cc:objc_library.bzl", "objc_library")
load("//third_party/nearby:minimum_os.bzl", "IOS_MINIMUM_OS")
load("//tools/build_defs/swift:swift_explicit_module_build_test.bzl", "swift_explicit_module_build_test")

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
    copts = [
        "-Wno-enum-compare",  # TODO(b/418286948): Remove this when the error is fixed.
    ],
    deps = [
        ":Shared",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
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
    copts = [
        "-Wno-enum-compare",  # TODO(b/418286948): Remove this when the error is fixed.
    ],
    deps = [
        ":Shared",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:CoreFoundation",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:QuartzCore",
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
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:CoreFoundation",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:QuartzCore",
    ],
)

objc_library(
    name = "CentralTestsLib",
    testonly = 1,
    srcs = [
        "Tests/Central/GNSCentralManagerTest.m",
    ],
    deps = [
        ":Central",
        ":Shared",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/objective_c/ocmock/v3:OCMock",
    ],
)

ios_unit_test(
    name = "CentralTests",
    google_create_srl_bindings_module = False,
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [":CentralTestsLib"],
)

objc_library(
    name = "PeripheralTestsLib",
    testonly = 1,
    srcs = [
        "Tests/Peripheral/GNSFakePeripheralManager.m",
        "Tests/Peripheral/GNSPeripheralManagerTest.m",
        "Tests/Peripheral/GNSPeripheralServiceManagerTest.m",
    ],
    hdrs = [
        "Tests/Peripheral/GNSFakePeripheralManager.h",
    ],
    deps = [
        ":Peripheral",
        ":Shared",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:XCTest",
        "//third_party/objective_c/ocmock/v3:OCMock",
    ],
)

ios_unit_test(
    name = "PeripheralTests",
    google_create_srl_bindings_module = False,
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [":PeripheralTestsLib"],
)

objc_library(
    name = "SharedTestsLib",
    testonly = 1,
    srcs = [
        "Tests/Shared/GNSSocketTest.m",
        "Tests/Shared/GNSWeavePacketTest.m",
    ],
    deps = [
        ":Shared",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/objective_c/ocmock/v3:OCMock",
    ],
)

ios_unit_test(
    name = "SharedTests",
    google_create_srl_bindings_module = False,
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [":SharedTestsLib"],
)

swift_explicit_module_build_test(
    name = "swift_explicit_module_build_test",
    ignore_headerless_targets = True,
    minimum_os_version = IOS_MINIMUM_OS,
    platform_type = "ios",
)
