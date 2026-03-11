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

package(default_visibility = ["//internal/platform/implementation/apple:__subpackages__"])

objc_library(
    name = "BLETestsLib",
    testonly = 1,
    srcs = [
        "GNCBLEGATTCharacteristicTest.mm",
        "GNCBLEGATTClientTest.m",
        "GNCBLEGATTServerTest.mm",
        "GNCBLEL2CAPClientTest.m",
        "GNCBLEL2CAPConnectionTest.m",
        "GNCBLEL2CAPFakeInputOutputStream.m",
        "GNCBLEL2CAPServerTest.mm",
        "GNCBLEL2CAPStreamTest.m",
        "GNCBLEMediumTest.m",
        "GNCFakeBLEGATTServer.m",
        "GNCFakeBLEMedium.m",
        "GNCFakeCBL2CAPChannel.m",
        "GNCFakeCentralManager.m",
        "GNCFakePeripheral.m",
        "GNCFakePeripheralManager.m",
        "GNCFakeSocket.m",
        "GNCMBleConnectionTest.m",
        "GNCMBleUtilsTest.m",
        "GNCMConnectionsTest.m",
        "GNCMFakeConnection.mm",
        "GNCPeripheralManagerMultiplexerTest.m",
        "GNCPeripheralManagerTest.m",
        "GNCPeripheralTest.m",
        "NSData+GNCBase85Test.m",
        "NSData+GNCWebSafeBase64Test.m",
    ],
    hdrs = [
        "GNCBLEGATTClient+Testing.h",
        "GNCBLEGATTServer+Testing.h",
        "GNCBLEL2CAPClient+Testing.h",
        "GNCBLEL2CAPFakeInputOutputStream.h",
        "GNCBLEL2CAPServer+Testing.h",
        "GNCBLEMedium+Testing.h",
        "GNCFakeBLEGATTServer.h",
        "GNCFakeBLEMedium.h",
        "GNCFakeCBL2CAPChannel.h",
        "GNCFakeCentralManager.h",
        "GNCFakePeripheral.h",
        "GNCFakePeripheralManager.h",
        "GNCFakeSocket.h",
        "GNCMFakeConnection.h",
    ],
    deps = [
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform/implementation/apple",  # buildcleaner: keep
        "//internal/platform/implementation/apple/Mediums/BLE",
        "//internal/platform/implementation/apple/Mediums/BLE/Sockets:Shared",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:ObjectiveC",
        "//third_party/apple_frameworks:XCTest",
    ],
)

ios_unit_test(
    name = "BLETests",
    google_create_srl_bindings_module = False,
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [":BLETestsLib"],
)
