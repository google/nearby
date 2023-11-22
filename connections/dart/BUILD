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

# load("//third_party/lexan/build_defs:lexan.bzl", "lexan")

package(default_visibility = ["//location/nearby:__subpackages__"])

licenses(["notice"])

# Build with --config=lexan
# lexan.cc_windows_dll(
cc_binary(
    name = "nearby_connections_dart",
    srcs = [
        "core_adapter_dart.cc",
        "core_adapter_dart.h",
        "core_adapter_sharp.cc",
        "core_adapter_sharp.h",
    ],
    linkshared = 1,
    tags = ["windows-dll"],
    deps = [
        "//connections:core",
        "//connections/c",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform/implementation/windows",
        "@com_google_absl//absl/synchronization",
        "@dart_lang//:dart_api_dl",
    ],
)
