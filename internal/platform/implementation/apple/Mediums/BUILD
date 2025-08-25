load("@rules_cc//cc:objc_library.bzl", "objc_library")

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
licenses(["notice"])

package(default_visibility = [
    "//connections:__subpackages__",
    "//googlemac/iPhone/Nearby:__subpackages__",
    "//internal/platform/implementation/apple:__subpackages__",
])

objc_library(
    name = "Mediums",
    srcs = [
        "BLEv2/GNCBLEError.m",
        "BLEv2/GNCBLEGATTCharacteristic.m",
        "BLEv2/GNCBLEGATTClient.m",
        "BLEv2/GNCBLEGATTServer.m",
        "BLEv2/GNCBLEL2CAPClient.m",
        "BLEv2/GNCBLEL2CAPConnection.m",
        "BLEv2/GNCBLEL2CAPServer.m",
        "BLEv2/GNCBLEL2CAPStream.m",
        "BLEv2/GNCBLEMedium.m",
        "BLEv2/GNCCentralManager.m",
        "BLEv2/GNCPeripheral.m",
        "BLEv2/GNCPeripheralManager.m",
        "BLEv2/NSData+GNCBase85.mm",
        "BLEv2/NSData+GNCWebSafeBase64.m",
        "Ble/GNCMBleConnection.m",
        "Ble/GNCMBleUtils.mm",
        "GNCLeaks.h",
        "GNCLeaks.m",
        "GNCMConnection.m",
        "Hotspot/GNCHotspotMedium.m",
        "Hotspot/GNCHotspotSocket.m",
    ],
    hdrs = [
        "BLEv2/GNCBLEError.h",
        "BLEv2/GNCBLEGATTCharacteristic.h",
        "BLEv2/GNCBLEGATTClient.h",
        "BLEv2/GNCBLEGATTServer.h",
        "BLEv2/GNCBLEL2CAPClient.h",
        "BLEv2/GNCBLEL2CAPConnection.h",
        "BLEv2/GNCBLEL2CAPServer.h",
        "BLEv2/GNCBLEL2CAPStream.h",
        "BLEv2/GNCBLEMedium.h",
        "BLEv2/GNCCentralManager.h",
        "BLEv2/GNCPeripheral.h",
        "BLEv2/GNCPeripheralManager.h",
        "BLEv2/NSData+GNCBase85.h",
        "BLEv2/NSData+GNCWebSafeBase64.h",
        "Ble/GNCMBleConnection.h",
        "Ble/GNCMBleUtils.h",
        "GNCMConnection.h",
        "Hotspot/GNCHotspotMedium.h",
        "Hotspot/GNCHotspotSocket.h",
    ],
    deps = [
        "//internal/encoding:base85",
        "//internal/platform/implementation/apple:Shared",
        "//internal/platform/implementation/apple/Flags",
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Shared",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//proto/mediums:ble_frames_cc_proto",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:CoreLocation",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/apple_frameworks:NetworkExtension",
        "//third_party/apple_frameworks:ObjectiveC",
        "//third_party/apple_frameworks:Security",
        "//third_party/apple_frameworks:SystemConfiguration",
    ],
)
