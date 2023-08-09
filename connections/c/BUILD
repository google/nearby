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
load("expand_version_template.bzl", "expand_version_template")
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = [
    "//connections:__subpackages__",
    "//location/nearby:__subpackages__",
])

licenses(["notice"])

cc_library(
    name = "types",
    hdrs = [
        "dll_config.h",
    ],
    defines = ["CORE_ADAPTER_DLL"],
    visibility = ["//visibility:private"],
    deps = ["@com_google_absl//absl/strings"],
)

bzl_library(
    name = "expand_version_template_bzl",
    srcs = ["expand_version_template.bzl"],
    parse_tests = False,
    visibility = ["//visibility:private"],
)

# Default version if none is provided.
vardef("VERSION", "1.0.0.0")

# When built with rapid, a version value will be passed down from the build config via blaze
# When built manually, invoke blaze with --define=VERSION=1.2.3.4
# If VERSION is not passed from blaze, the default value defined above will be used.
expand_version_template(
    name = "version_expanded",
    out = "version.rc",
    template = "version.rc.tpl",
    version = varref("VERSION"),
)

lexan.resource_files(
    name = "resources",
    rc_files = [
        ":version_expanded",
    ],
)

cc_library(
    name = "c",
    srcs = [
        "advertising_options_w.cc",
        "connection_options_w.cc",
        "core_adapter.cc",
        "discovery_options_w.cc",
        "file_w.cc",
        "listeners_w.cc",
        "payload_w.cc",
        "strategy_w.cc",
    ],
    hdrs = [
        "advertising_options_w.h",
        "connection_options_w.h",
        "core_adapter.h",
        "discovery_options_w.h",
        "file_w.h",
        "listeners_w.h",
        "medium_selector_w.h",
        "options_base_w.h",
        "out_of_band_connection_metadata_w.h",
        "params_w.h",
        "payload_w.h",
        "strategy_w.h",
    ],
    deps = [
        ":types",
        "//connections:core",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform:types",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

# Build with --config=lexan
lexan.cc_windows_dll(
    name = "nearby_connections",
    tags = ["windows-dll"],
    deps = [
        ":c",
        ":types",
        "//internal/platform/implementation/windows",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "connections_test",
    size = "small",
    srcs = [
        "bluetooth_classic_server_socket_test.cc",
    ],
    deps = [
        "//connections:core",
        "//internal/platform:types",
        "//internal/platform/implementation/windows",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
