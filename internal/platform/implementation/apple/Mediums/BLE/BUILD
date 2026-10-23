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
    name = "BLE",
    srcs = [
        "GNCBLEError.m",
        "GNCBLEGATTCharacteristic.m",
        "GNCBLEGATTClient.m",
        "GNCBLEGATTServer.m",
        "GNCBLEL2CAPClient.m",
        "GNCBLEL2CAPConnection.m",
        "GNCBLEL2CAPServer.m",
        "GNCBLEL2CAPStream.m",
        "GNCBLEMedium.m",
        "GNCCentralManager.m",
        "GNCLeaks.m",
        "GNCMBleConnection.m",
        "GNCMBleUtils.mm",
        "GNCMConnection.m",
        "GNCPeripheral.m",
        "GNCPeripheralManager.m",
        "GNCPeripheralManagerMultiplexer.m",
        "NSData+GNCBase85.mm",
        "NSData+GNCWebSafeBase64.m",
    ],
    hdrs = [
        "GNCBLEError.h",
        "GNCBLEGATTCharacteristic.h",
        "GNCBLEGATTClient.h",
        "GNCBLEGATTServer.h",
        "GNCBLEL2CAPClient.h",
        "GNCBLEL2CAPConnection.h",
        "GNCBLEL2CAPServer.h",
        "GNCBLEL2CAPStream.h",
        "GNCBLEMedium.h",
        "GNCCentralManager.h",
        "GNCLeaks.h",
        "GNCMBleConnection.h",
        "GNCMBleUtils.h",
        "GNCMConnection.h",
        "GNCPeripheral.h",
        "GNCPeripheralManager.h",
        "GNCPeripheralManagerMultiplexer.h",
        "NSData+GNCBase85.h",
        "NSData+GNCWebSafeBase64.h",
    ],
    deps = [
        "//internal/encoding:base85",
        "//internal/platform/implementation/apple:Shared",
        "//internal/platform/implementation/apple/Flags",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//internal/platform/implementation/apple/Mediums/BLE/Sockets:Shared",
        "//proto/mediums:ble_frames_cc_proto",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:ObjectiveC",
    ],
)
