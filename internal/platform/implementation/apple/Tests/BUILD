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
load("//third_party/nearby:minimum_os.bzl", "IOS_LATEST_TEST_RUNNER", "IOS_MINIMUM_OS")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

objc_library(
    name = "PlatformTestslib",
    testonly = True,
    srcs = [
        "GNCBLEGATTCharacteristicTest.mm",
        "GNCBLEGATTClient+Testing.h",
        "GNCBLEGATTClientTest.m",
        "GNCBLEGATTServer+Testing.h",
        "GNCBLEGATTServerTest.m",
        "GNCBLEL2CAPClient+Testing.h",
        "GNCBLEL2CAPClientTest.m",
        "GNCBLEL2CAPConnectionTest.m",
        "GNCBLEL2CAPFakeInputOutputStream.h",
        "GNCBLEL2CAPFakeInputOutputStream.m",
        "GNCBLEL2CAPServerTest.m",
        "GNCBLEL2CAPStreamTest.m",
        "GNCBLEMedium+Testing.h",
        "GNCBLEMediumTest.m",
        "GNCBLEUtilsTest.mm",
        "GNCCryptoTest.mm",
        "GNCDeviceInfoTest.mm",
        "GNCFakeCentralManager.h",
        "GNCFakeCentralManager.m",
        "GNCFakePeripheral.h",
        "GNCFakePeripheral.m",
        "GNCFakePeripheralManager.h",
        "GNCFakePeripheralManager.m",
        "GNCMultiThreadExecutorTest.mm",
        "GNCPlatformTest.mm",
        "GNCScheduledExecutorTest.mm",
        "GNCSingleThreadExecutorTest.mm",
        "GNCUtilsTest.m",
        "NSData+GNCBase85Test.m",
        "NSData+GNCWebSafeBase64Test.m",
        "UtilsTest.mm",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/apple",  # buildcleaner: keep
        "//internal/platform/implementation/apple:Shared",
        "//internal/platform/implementation/apple:ble_v2",
        "//internal/platform/implementation/apple:comm",
        "//internal/platform/implementation/apple/Mediums",
        "//third_party/apple_frameworks:CommonCrypto",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:XCTest",
        "@com_google_absl//absl/time",
    ],
)

ios_unit_test(
    name = "PlatformTests",
    minimum_os_version = IOS_MINIMUM_OS,
    runner = IOS_LATEST_TEST_RUNNER,
    deps = [
        ":PlatformTestslib",
    ],
)
