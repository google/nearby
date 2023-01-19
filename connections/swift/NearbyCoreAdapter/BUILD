# Copyright 2022 Google LLC
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

load("//tools/build_defs/apple:ios.bzl", "ios_unit_test")
load("//tools/build_defs/swift:swift_library.bzl", "swift_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

objc_library(
    name = "NearbyCoreAdapter",
    srcs = glob([
        "Sources/*.mm",
        "Sources/*.m",
        "Sources/*.h",
    ]),
    hdrs = glob(["Sources/Public/**/*.h"]),
    features = ["-layering_check"],
    module_name = "NearbyCoreAdapter",
    deps = [
        "//connections:core",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform/implementation/apple",  # buildcleaner: keep
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:ObjectiveC",
    ],
)

swift_library(
    name = "TestsLib",
    testonly = 1,
    srcs = ["Tests/BuildTests.swift"],
    deps = [
        ":NearbyCoreAdapter",
        "//third_party/apple_frameworks:XCTest",
    ],
)

ios_unit_test(
    name = "Tests",
    minimum_os_version = "12.0",
    runner = "//testing/utp/ios:IOS_12",
    deps = [
        ":TestsLib",
    ],
)