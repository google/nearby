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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "wifi_direct",
    textual_hdrs = ["wifi_direct.h"],
    visibility = [
        "//connections/implementation:__pkg__",
        "//connections/implementation/mediums:__pkg__",
    ],
    deps = [
        ":wifi_direct_internal",
    ],
)

cc_library(
    name = "wifi_direct_internal",
    srcs = [
        "wifi_direct.cc",
        "wifi_direct_bwu_handler.cc",
        "wifi_direct_endpoint_channel.cc",
    ],
    hdrs = [
        "wifi_direct.h",
        "wifi_direct_bwu_handler.h",
        "wifi_direct_endpoint_channel.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//connections:core_types",
        "//connections/implementation:bwu_handler",
        "//connections/implementation:client_proxy",
        "//connections/implementation:endpoint_channel",
        "//connections/implementation:offline_frames",
        "//internal/base:masker",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "wifi_direct_test",
    size = "small",
    srcs = [
        "wifi_direct_test.cc",
    ],
    deps = [
        ":wifi_direct_internal",
        "//connections/implementation:endpoint_channel",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:test_util",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wifi_direct_bwu_handler_test",
    size = "small",
    srcs = [
        "wifi_direct_bwu_handler_test.cc",
    ],
    deps = [
        ":wifi_direct_internal",
        "//connections/implementation:bwu_handler",
        "//connections/implementation:client_proxy",
        "//connections/implementation:endpoint_channel",
        "//connections/implementation:offline_frames",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
