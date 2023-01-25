load("//tools/build_defs/apple:ios.bzl", "ios_unit_test")

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

package(default_visibility = ["//visibility:public"])

objc_library(
    name = "PlatformTestslib",
    testonly = 1,
    srcs = [
        "GNCBleTest.mm",
        "GNCBluetoothAdapterTest.mm",
        "GNCCryptoTest.mm",
        "GNCMultiThreadExecutorTest.mm",
        "GNCScheduledExecutorTest.mm",
        "GNCSingleThreadExecutorTest.mm",
        "GNCUtilsTest.mm",
    ],
    features = ["-layering_check"],
    deps = [
        "//internal/platform/implementation/apple",
        "//internal/platform/implementation/apple:Shared",
    ],
)

ios_unit_test(
    name = "PlatformTests",
    minimum_os_version = "13.0",
    runner = "//testing/utp/ios:IOS_13",
    deps = [
        ":PlatformTestslib",
    ],
)
