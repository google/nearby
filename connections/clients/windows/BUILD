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
load("//third_party/lexan/build_defs:lexan.bzl", "lexan")

licenses(["notice"])

cc_library(
    name = "types",
    hdrs = [
        "dll_config.h",
    ],
    visibility = [
        "//location/nearby/cpp/sharing/implementation:__subpackages__",
        "//connections:__subpackages__",
        "//connections/clients/windows:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/platform/implementation:__subpackages__",
    ],
    deps = [
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/strings",
    ],
)

# Build with --config=lexan
lexan.cc_windows_dll(
    name = "core_adapter",
    srcs = [
        "advertising_options_w.cc",
        "connection_options_w.cc",
        "core_adapter.cc",
        "discovery_options_w.cc",
        "listeners_w.cc",
        "payload_w.cc",
        "strategy_w.cc",
    ],
    hdrs = [
        "advertising_options_w.h",
        "connection_options_w.h",
        "core_adapter.h",
        "discovery_options_w.h",
        "dll_config.h",
        "listeners_w.h",
        "medium_selector_w.h",
        "options_base_w.h",
        "out_of_band_connection_metadata_w.h",
        "params_w.h",
        "payload_w.h",
        "strategy_w.h",
    ],
    copts = ["-DCORE_ADAPTER_BUILD_DLL -DCOMPILING_CORE"],
    defines = ["CORE_ADAPTER_DLL"],
    hdrs_deps = ["//connections:core"],
    tags = ["windows-dll"],
    visibility = ["//location/nearby/testing:__subpackages__"],
    deps = [
        "//third_party/dart_lang/v2:dart_api_dl",
        "//connections:core",
        "//connections:core_types",
        "//connections/clients/windows:types",
        "//internal/platform:base",
        "//internal/platform/implementation/windows",
        "//third_party/webrtc/files/stable/webrtc/api:create_peerconnection_factory",
        "@com_google_absl//absl/strings",
    ],
)

# Build with --config=lexan
# TODO(b/203019344) Move this configuration under dart folder and create a BUILD there.
lexan.cc_windows_dll(
    name = "core_adapter_dart",
    srcs = [
        "advertising_options_w.cc",
        "connection_options_w.cc",
        "core_adapter.cc",
        "dart/core_adapter_dart.cc",
        "discovery_options_w.cc",
        "listeners_w.cc",
        "payload_w.cc",
        "strategy_w.cc",
    ],
    hdrs = [
        "advertising_options_w.h",
        "connection_options_w.h",
        "core_adapter.h",
        "dart/core_adapter_dart.h",
        "discovery_options_w.h",
        "listeners_w.h",
        "medium_selector_w.h",
        "options_base_w.h",
        "out_of_band_connection_metadata_w.h",
        "params_w.h",
        "payload_w.h",
        "strategy_w.h",
    ],
    copts = ["-DCORE_ADAPTER_DART_BUILD_DLL -DCORE_ADAPTER_DLL -DCOMPILING_CORE"],
    defines = ["CORE_ADAPTER_DART_DLL"],
    tags = ["windows-dll"],
    deps = [
        "//third_party/dart_lang/v2:dart_api_dl",
        "//connections:core",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform/implementation/windows",
        "//third_party/webrtc/files/stable/webrtc/api:create_peerconnection_factory",
        "@com_google_absl//absl/strings",
    ],
)
