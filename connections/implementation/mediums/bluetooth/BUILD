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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "bluetooth",
    textual_hdrs = ["bluetooth_classic.h"],
    visibility = [
        "//connections/implementation:__pkg__",
        "//connections/implementation/mediums:__pkg__",
    ],
    deps = [
        ":bluetooth_internal",
    ],
)

cc_library(
    name = "bluetooth_internal",
    srcs = [
        "bluetooth_bwu_handler.cc",
        "bluetooth_classic.cc",
        "bluetooth_endpoint_channel.cc",
    ],
    hdrs = [
        "bluetooth_bwu_handler.h",
        "bluetooth_classic.h",
        "bluetooth_endpoint_channel.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//connections:core_types",
        "//connections/implementation:bwu_handler",
        "//connections/implementation:client_proxy",
        "//connections/implementation:endpoint_channel",
        "//connections/implementation:offline_frames",
        "//connections/implementation/mediums:bluetooth_radio",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

cc_test(
    name = "bluetooth_test",
    size = "small",
    srcs = [
        "bluetooth_bwu_handler_test.cc",
        "bluetooth_classic_test.cc",
    ],
    deps = [
        ":bluetooth_internal",
        "//connections/implementation:bwu_handler",
        "//connections/implementation:client_proxy",
        "//connections/implementation:endpoint_channel",
        "//connections/implementation:offline_frames",
        "//connections/implementation/mediums",
        "//connections/implementation/mediums:bluetooth_radio",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
