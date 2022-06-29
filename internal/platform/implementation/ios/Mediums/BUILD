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

package(default_visibility = ["//internal/platform/implementation/ios:__subpackages__"])

objc_library(
    name = "Mediums",
    srcs = [
        "Ble/GNCMBleCentral.m",
        "Ble/GNCMBleConnection.m",
        "Ble/GNCMBlePeripheral.m",
        "Ble/GNCMBleUtils.mm",
        "GNCLeaks.m",
        "GNCMConnection.m",
        "WifiLan/GNCMBonjourBrowser.m",
        "WifiLan/GNCMBonjourConnection.m",
        "WifiLan/GNCMBonjourService.m",
        "WifiLan/GNCMBonjourUtils.m",
    ],
    hdrs = [
        "Ble/GNCMBleCentral.h",
        "Ble/GNCMBleConnection.h",
        "Ble/GNCMBlePeripheral.h",
        "Ble/GNCMBleUtils.h",
        "GNCLeaks.h",
        "GNCMConnection.h",
        "WifiLan/GNCMBonjourBrowser.h",
        "WifiLan/GNCMBonjourConnection.h",
        "WifiLan/GNCMBonjourService.h",
        "WifiLan/GNCMBonjourUtils.h",
    ],
    deps = [
        "//third_party/apple_frameworks:CoreBluetooth",
        "//third_party/apple_frameworks:Foundation",
        "//internal/platform/implementation/ios:Shared",
        "//internal/platform/implementation/ios/Mediums/Ble/Sockets:Central",
        "//internal/platform/implementation/ios/Mediums/Ble/Sockets:Peripheral",
        "//internal/platform/implementation/ios/Mediums/Ble/Sockets:Shared",
        "//proto/mediums:ble_frames_cc_proto",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
        "@com_google_absl//absl/numeric:int128",
    ],
)
