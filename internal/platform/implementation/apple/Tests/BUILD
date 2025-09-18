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
    name = "PlatformTestsLibrary",
    testonly = True,
    srcs = [
        "GNCAwdlMediumTest.mm",
        "GNCBLEUtilsTest.mm",
        "GNCCryptoTest.mm",
        "GNCDeviceInfoTest.mm",
        "GNCMultiThreadExecutorTest.mm",
        "GNCPlatformTest.mm",
        "GNCPreferencesManagerTest.mm",
        "GNCScheduledExecutorTest.mm",
        "GNCSingleThreadExecutorTest.mm",
        "GNCTimerTest.mm",
        "GNCUtilsTest.m",
        "GNCWifiLanMediumTest.mm",
        "UtilsTest.mm",
        "ble_peripheral_test.mm",
        "ble_socket_test.mm",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/apple",  # buildcleaner: keep
        "//internal/platform/implementation/apple:Shared",
        "//internal/platform/implementation/apple:ble_v2",
        "//internal/platform/implementation/apple/Mediums/BLE",
        "//internal/platform/implementation/apple/Mediums/BLE/Tests:BLETestsLib",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon/Tests:FakeNWFramework",
        "//third_party/apple_frameworks:CommonCrypto",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:XCTest",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@nlohmann_json//:json",
    ],
)

objc_library(
    name = "BluetoothAdapterV2TestLibrary",
    testonly = True,
    srcs = ["bluetooth_adapter_v2_test.mm"],
    tags = ["nozapfhahn"],
    deps = [
        "//internal/platform:mac_address",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/apple:bluetooth_adapter_v2",
        "//third_party/apple_frameworks:XCTest",
        "@com_google_googletest//:gtest",
    ],
)

ios_unit_test(
    name = "PlatformTest",
    minimum_os_version = IOS_MINIMUM_OS,
    runner = IOS_LATEST_TEST_RUNNER,
    deps = [
        ":BluetoothAdapterV2TestLibrary",
        ":PlatformTestsLibrary",
    ],
)
