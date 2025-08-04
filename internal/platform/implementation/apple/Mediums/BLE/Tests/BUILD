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

load("@rules_apple//apple:ios.bzl", "ios_unit_test")
load("@rules_cc//cc:objc_library.bzl", "objc_library")
load("//third_party/nearby:minimum_os.bzl", "IOS_MINIMUM_OS")

licenses(["notice"])

package(default_visibility = ["//:__subpackages__"])

objc_library(
    name = "BLETestsLib",
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
        "GNCFakeCentralManager.h",
        "GNCFakeCentralManager.m",
        "GNCFakePeripheral.h",
        "GNCFakePeripheral.m",
        "GNCFakePeripheralManager.h",
        "GNCFakePeripheralManager.m",
        "NSData+GNCBase85Test.m",
        "NSData+GNCWebSafeBase64Test.m",
    ],
    deps = [
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/apple:ble",
        "//internal/platform/implementation/apple/Mediums/BLE",
        "//proto/mediums:ble_frames_cc_proto",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:XCTest",
    ],
)

ios_unit_test(
    name = "BLETests",
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [":BLETestsLib"],
)
