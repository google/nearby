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
load("//third_party/nearby:minimum_os.bzl", "IOS_MINIMUM_OS")

package(default_visibility = ["//:__subpackages__"])

objc_library(
    name = "HotspotTestslib",
    testonly = True,
    srcs = [
        "GNCHotspotMediumTest.m",
    ],
    deps = [
        "//internal/platform/implementation/apple/Mediums/Hotspot",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//third_party/apple_frameworks:CoreLocation",
        "//third_party/apple_frameworks:NetworkExtension",
        "//third_party/apple_frameworks:XCTest",
    ],
)

ios_unit_test(
    name = "HotspotTests",
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [
        ":HotspotTestslib",
    ],
)
