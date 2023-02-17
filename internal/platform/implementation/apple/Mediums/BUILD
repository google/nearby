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
        "Ble/GNCMBleCentral.m",
        "Ble/GNCMBleConnection.m",
        "Ble/GNCMBlePeripheral.m",
        "Ble/GNCMBleUtils.mm",
        "GNCLeaks.h",
        "GNCLeaks.m",
        "GNCMConnection.m",
        "WiFiLAN/GNCWiFiLANError.m",
        "WiFiLAN/GNCWiFiLANMedium.m",
        "WiFiLAN/GNCWiFiLANServerSocket.m",
        "WiFiLAN/GNCWiFiLANServerSocket+Internal.h",
        "WiFiLAN/GNCWiFiLANSocket.m",
    ],
    hdrs = [
        "Ble/GNCMBleCentral.h",
        "Ble/GNCMBleConnection.h",
        "Ble/GNCMBlePeripheral.h",
        "Ble/GNCMBleUtils.h",
        "GNCMConnection.h",
        "WiFiLAN/GNCWiFiLANError.h",
        "WiFiLAN/GNCWiFiLANMedium.h",
        "WiFiLAN/GNCWiFiLANServerSocket.h",
        "WiFiLAN/GNCWiFiLANSocket.h",
    ],
    deps = [
        "//internal/platform/implementation/apple:Shared",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Central",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Peripheral",
        "//internal/platform/implementation/apple/Mediums/Ble/Sockets:Shared",
        "//proto/mediums:ble_frames_cc_proto",
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ],
)
