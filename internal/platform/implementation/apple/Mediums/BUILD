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
licenses(["notice"])

package(default_visibility = ["//internal/platform/implementation/apple:__subpackages__"])

objc_library(
    name = "Mediums",
    srcs = [
        "BLEv2/GNCBLEError.m",
        "BLEv2/GNCBLEGATTCharacteristic.m",
        "BLEv2/GNCBLEGATTClient.m",
        "BLEv2/GNCBLEGATTServer.m",
        "BLEv2/GNCBLEMedium.m",
        "BLEv2/GNCCentralManager.m",
        "BLEv2/GNCPeripheral.m",
        "BLEv2/GNCPeripheralManager.m",
        "BLEv2/NSData+GNCWebSafeBase64.m",
        "Ble/GNCMBleConnection.m",
        "Ble/GNCMBleUtils.mm",
        "GNCLeaks.h",
        "GNCLeaks.m",
        "GNCMConnection.m",
        "WiFiLAN/GNCIPv4Address.m",
        "WiFiLAN/GNCWiFiLANError.m",
        "WiFiLAN/GNCWiFiLANMedium.m",
        "WiFiLAN/GNCWiFiLANServerSocket.m",
        "WiFiLAN/GNCWiFiLANServerSocket+Internal.h",
        "WiFiLAN/GNCWiFiLANSocket.m",
    ],
    hdrs = [
        "BLEv2/GNCBLEError.h",
        "BLEv2/GNCBLEGATTCharacteristic.h",
        "BLEv2/GNCBLEGATTClient.h",
        "BLEv2/GNCBLEGATTServer.h",
        "BLEv2/GNCBLEMedium.h",
        "BLEv2/GNCCentralManager.h",
        "BLEv2/GNCPeripheral.h",
        "BLEv2/GNCPeripheralManager.h",
        "BLEv2/NSData+GNCWebSafeBase64.h",
        "Ble/GNCMBleConnection.h",
        "Ble/GNCMBleUtils.h",
        "GNCMConnection.h",
        "WiFiLAN/GNCIPv4Address.h",
        "WiFiLAN/GNCWiFiLANError.h",
        "WiFiLAN/GNCWiFiLANMedium.h",
        "WiFiLAN/GNCWiFiLANServerSocket.h",
        "WiFiLAN/GNCWiFiLANSocket.h",
    ],
    deps = [
        "//internal/platform/implementation/apple:Shared",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Shared",
        "//proto/mediums:ble_frames_cc_proto",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/apple_frameworks:ObjectiveC",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ],
)
