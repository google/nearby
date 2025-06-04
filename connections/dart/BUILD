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

load("//third_party/cpptoolchains/windows_llvm/build_defs:windows.bzl", "windows")

package(default_visibility = ["//location/nearby:__subpackages__"])

licenses(["notice"])

# Build with --config=windows
windows.cc_windows_dll(
    name = "nc_windows_dart",
    srcs = [
        "nc_adapter_dart.cc",
        "nearby_connections_client_state.cc",
        "shared_buffer_stream.cc",
    ],
    hdrs = [
        "nc_adapter_dart.h",
        "nc_adapter_def.h",
        "nc_adapter_types.h",
        "nearby_connections_client_state.h",
        "shared_buffer_stream.h",
    ],
    copts = ["-DNC_DART_DLL"],
    tags = ["windows-dll"],
    deps = [
        ":shared_buffer_manager",
        "//connections/c:nc_types",
        "//connections/c:nc_windows",
        "//internal/platform:types",
        "//internal/platform/implementation/windows",
        "//third_party/dart_lang/v2:dart_api_dl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
    ],
)

objc_library(
    name = "nearby_connections_objc",
    srcs = [
        "nc_adapter_dart.cc",
        "nearby_connections_client_state.cc",
        "shared_buffer_stream.cc",
    ],
    hdrs = [
        "nc_adapter_dart.h",
        "nc_adapter_def.h",
        "nc_adapter_types.h",
        "nearby_connections_client_state.h",
        "shared_buffer_stream.h",
    ],
    deps = [
        ":shared_buffer_manager",
        "//connections/c:nc",
        "//connections/c:nc_types",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/apple",  # buildcleaner: keep
        "//third_party/dart_lang/v2:dart_api_dl",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
    ],
    alwayslink = 1,
)

# C++ interface for shared buffer manager
cc_library(
    name = "shared_buffer_manager",
    hdrs = ["shared_buffer_manager.h"],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [],  # No dependencies for the interface
)

cc_library(
    name = "shared_buffer_stream",
    srcs = [
        "shared_buffer_stream.cc",
    ],
    hdrs = [
        "shared_buffer_stream.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:public"],
    deps = [
        # Add dependency here if shared_buffer_stream depends on other library.
        ":shared_buffer_manager",
        "@com_google_absl//absl/synchronization",
        "//internal/platform:base",
        "//internal/platform:logging",
    ],
)
