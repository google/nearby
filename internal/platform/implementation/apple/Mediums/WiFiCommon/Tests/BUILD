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
    name = "FakeNWFramework",
    testonly = True,
    srcs = [
        "GNCFakeNWBrowseResult.m",
        "GNCFakeNWBrowser.m",
        "GNCFakeNWConnection.m",
        "GNCFakeNWFramework.m",
        "GNCFakeNWFrameworkServerSocket.m",
        "GNCFakeNWFrameworkSocket.m",
        "GNCFakeNWListener.m",
    ],
    hdrs = [
        "GNCFakeNWBrowseResult.h",
        "GNCFakeNWBrowser.h",
        "GNCFakeNWConnection.h",
        "GNCFakeNWFramework.h",
        "GNCFakeNWFrameworkServerSocket.h",
        "GNCFakeNWFrameworkSocket.h",
        "GNCFakeNWListener.h",
    ],
    deps = [
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
    ],
)

objc_library(
    name = "WiFiCommonTestsLib",
    testonly = True,
    srcs = [
        "GNCIPAddressTest.mm",
        "GNCNWBrowseResultImplTest.m",
        "GNCNWBrowserImplTest.m",
        "GNCNWConnectionImplTest.m",
        "GNCNWFrameworkServerSocketTest.m",
        "GNCNWFrameworkSocketTest.m",
        "GNCNWFrameworkTest.m",
        "GNCNWListenerImplTest.m",
        "GNCNWParametersTest.m",
    ],
    deps = [
        ":FakeNWFramework",
        "//internal/platform/implementation/apple/Mediums/WiFiCommon",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/apple_frameworks:XCTest",
        "//third_party/objective_c/ocmock/v3:OCMock",
    ],
)

ios_unit_test(
    name = "WiFiCommonTests",
    minimum_os_version = IOS_MINIMUM_OS,
    runner = "//testing/utp/ios:IOS_LATEST",
    deps = [":WiFiCommonTestsLib"],
)
