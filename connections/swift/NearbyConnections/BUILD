# Copyright 2022-2023 Google LLC
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

load("//third_party/nearby:minimum_os.bzl", "IOS_LATEST_TEST_RUNNER", "IOS_MINIMUM_OS")
load("//tools/build_defs/apple:ios.bzl", "ios_unit_test")
load("//tools/build_defs/swift:swift_library.bzl", "swift_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

swift_library(
    name = "NearbyConnections",
    srcs = glob(["Sources/**/*.swift"]),
    module_name = "NearbyConnections",
    deps = [
        "//connections/swift/NearbyCoreAdapter",
        "//third_party/apple_frameworks:Foundation",
    ],
)

swift_library(
    name = "TestsLib",
    testonly = 1,
    srcs = glob(["Tests/**/*.swift"]),
    deps = [
        ":NearbyConnections",
        "//third_party/apple_frameworks:XCTest",
    ],
)

ios_unit_test(
    name = "Tests",
    minimum_os_version = IOS_MINIMUM_OS,
    runner = IOS_LATEST_TEST_RUNNER,
    deps = [
        ":TestsLib",
    ],
)
