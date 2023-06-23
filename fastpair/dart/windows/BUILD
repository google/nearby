# Copyright 2023 Google LLC
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
load("//third_party/lexan/build_defs:lexan.bzl", "lexan")

licenses(["notice"])

lexan.cc_windows_dll(
    name = "fastpair_adapter",
    srcs = [
        "fast_pair_service_adapter.cc",
    ],
    hdrs = [
        "fast_pair_service_adapter.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    defines = [
        "LOG_SEVERITY_VERBOSE",
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
    ],
    tags = ["windows-dll"],
    visibility = [
        "//location/nearby/apps/better_together/windows/fast_pair:__subpackages__",
    ],
    deps = [
        "//fastpair/dart/proto:fastpair_cc_proto",
        "//fastpair/keyed_service",
        "//fastpair/ui:fast_pair_ui",
        "//internal/platform:logging",
        "//internal/platform/implementation/windows",
        "//internal/platform/implementation/windows/generated:types",
        "@com_google_absl//absl/strings",
    ],
)

lexan.cc_windows_dll(
    name = "fastpair_dart",
    srcs = [
        "fast_pair_service_adapter.cc",
        "fast_pair_service_adapter_dart.cc",
    ],
    hdrs = [
        "fast_pair_service_adapter.h",
        "fast_pair_service_adapter_dart.h",
    ],
    copts = [
        "-Wc++17-compat",
        "-Ithird_party",
    ],
    defines = [
        "LOG_SEVERITY_VERBOSE",
        "DART_SHARED_LIB=1",
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
    ],
    tags = ["windows-dll"],
    visibility = [
        "//location/nearby/apps/better_together/windows/fast_pair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/dart/proto:fastpair_cc_proto",
        "//fastpair/keyed_service",
        "//fastpair/ui:fast_pair_ui",
        "//internal/platform:logging",
        "//internal/platform/implementation/windows",
        "//internal/platform/implementation/windows/generated:types",
        "//third_party/dart_lang/v2:dart_api_dl",
        "@com_google_absl//absl/strings",
    ],
)
