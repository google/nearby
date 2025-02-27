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

load("@rules_apple//apple:apple.bzl", "apple_static_xcframework")
load("//third_party/cpptoolchains/windows_llvm/build_defs:windows.bzl", "windows")

package(default_visibility = [
    "//connections:__subpackages__",
    "//location/nearby:__subpackages__",
])

licenses(["notice"])

cc_library(
    name = "nc_types",
    hdrs = [
        "nc.h",
        "nc_def.h",
        "nc_types.h",
    ],
)

cc_library(
    name = "nc",
    srcs = [
        "nc.cc",
    ],
    copts = ["-DNC_DLL"],
    deps = [
        ":nc_types",
        "//connections:core",
        "//connections:core_types",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:flag_reader",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:types",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

# iOS only.
# Warning: Do not rename this target, as it will break Kokoro workflows.
apple_static_xcframework(
    name = "NearbyConnections",
    avoid_deps = [],
    bundle_name = "NearbyConnections",
    ios = {
        "simulator": [
            "arm64",
            "x86_64",
        ],
        "device": [
            "arm64",
        ],
    },
    minimum_os_versions = {
        "ios": "15.0",
    },
    deps = [
        ":nc",
        "//internal/platform/implementation/apple",
        "//internal/platform/implementation/apple/Mediums",
    ],
)

# Windows.
windows.cc_windows_dll(
    name = "nc_windows",
    srcs = [],
    copts = ["-DNC_DLL"],
    tags = ["windows-dll"],
    deps = [
        ":nc",
        "//internal/platform/implementation/windows",
        "@com_google_absl//absl/strings",
    ],
)

# macOS.
cc_binary(
    name = "libnc.so",
    linkshared = 1,
    linkstatic = 1,
    tags = ["notap"],
    deps = [
        ":nc",
        "//internal/platform/implementation/apple",
        "//internal/platform/implementation/apple/Mediums",
    ],
)
