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

licenses(["notice"])

cc_library(
    name = "base",
    srcs = [
    ],
    hdrs = [
        "observer_list.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
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
        "//fastpair:__subpackages__",
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
    name = "files",
    srcs = ["files.cc"],
    hdrs = ["files.h"],
    visibility = ["//visibility:public"],
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
    name = "files_test",
    size = "small",
    timeout = "short",
    srcs = [
        "files_test.cc",
    ],
    deps = [
        ":files",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
