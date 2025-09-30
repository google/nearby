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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "base",
    srcs = [
    ],
    hdrs = [
        "observer_list.h",
    ],
    visibility = [
        "//internal/account:__subpackages__",
        "//internal/platform:__pkg__",
        "//internal/test:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "//internal/platform:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "bluetooth_address",
    srcs = [
        "bluetooth_address.cc",
    ],
    hdrs = [
        "bluetooth_address.h",
    ],
    visibility = [
        "//internal:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "compatible_u8_string",
    hdrs = ["compatible_u8_string.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "masker",
    srcs = ["masker.cc"],
    hdrs = ["masker.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "file_path",
    srcs = ["file_path.cc"],
    hdrs = ["file_path.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":compatible_u8_string",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "files",
    srcs = ["files.cc"],
    hdrs = ["files.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":file_path",
        "//internal/platform:logging",
    ],
)

cc_test(
    name = "base_test",
    size = "small",
    timeout = "short",
    srcs = [
        "bluetooth_address_test.cc",
    ],
    deps = [
        ":bluetooth_address",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file_path_test",
    size = "small",
    timeout = "short",
    srcs = [
        "file_path_test.cc",
    ],
    deps = [
        ":file_path",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "files_test",
    size = "small",
    timeout = "short",
    srcs = [
        "files_test.cc",
    ],
    deps = [
        ":file_path",
        ":files",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "masker_test",
    size = "small",
    timeout = "short",
    srcs = [
        "masker_test.cc",
    ],
    deps = [
        ":masker",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
