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

load("@rules_cc//cc:objc_library.bzl", "objc_library")
load("//third_party/nearby:minimum_os.bzl", "IOS_MINIMUM_OS")
load("//tools/build_defs/swift:swift_explicit_module_build_test.bzl", "swift_explicit_module_build_test")

licenses(["notice"])

package(default_visibility = [
    "//internal/platform/implementation/apple:__subpackages__",
])

objc_library(
    name = "WiFiCommon",
    srcs = [
        "GNCIPv4Address.m",
        "GNCNWConnectionImpl.m",
        "GNCNWFramework.m",
        "GNCNWFrameworkError.m",
        "GNCNWFrameworkServerSocket.m",
        "GNCNWFrameworkSocket.m",
        "GNCNWListenerImpl.m",
        "GNCNWParameters.m",
    ],
    hdrs = [
        "GNCIPv4Address.h",
        "GNCNWConnection.h",
        "GNCNWConnectionImpl.h",
        "GNCNWFramework.h",
        "GNCNWFrameworkError.h",
        "GNCNWFrameworkServerSocket.h",
        "GNCNWFrameworkServerSocket+Internal.h",
        "GNCNWFrameworkSocket.h",
        "GNCNWListener.h",
        "GNCNWListenerImpl.h",
        "GNCNWParameters.h",
    ],
    deps = [
        "//internal/platform/implementation/apple/Log:GNCLogger",
        "//third_party/apple_frameworks:Foundation",
        "//third_party/apple_frameworks:Network",
        "//third_party/apple_frameworks:Security",
    ],
)

swift_explicit_module_build_test(
    name = "swift_explicit_module_build_test",
    ignore_headerless_targets = True,
    minimum_os_version = IOS_MINIMUM_OS,
    platform_type = "ios",
)
