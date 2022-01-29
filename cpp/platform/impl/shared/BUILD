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
licenses(["notice"])

cc_library(
    name = "posix_mutex",
    srcs = [
        "posix_mutex.cc",
    ],
    hdrs = [
        "posix_mutex.h",
    ],
    visibility = [
        "//cpp/platform/impl:__subpackages__",
    ],
    deps = ["//cpp/platform/api:types"],
)

cc_library(
    name = "posix_condition_variable",
    srcs = [
        "posix_condition_variable.cc",
    ],
    hdrs = [
        "posix_condition_variable.h",
    ],
    deps = [
        ":posix_mutex",
        "//cpp/platform/api:types",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    visibility = [
        "//cpp/platform/impl:__subpackages__",
    ],
    deps = [
        "//cpp/platform/api:types",
        "//cpp/platform/base",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "count_down_latch",
    srcs = ["count_down_latch.cc"],
    hdrs = ["count_down_latch.h"],
    visibility = [
        "//cpp/platform/impl:__subpackages__",
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
    ],
    deps = [
        "//cpp/platform/api:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "file_test",
    srcs = ["file_test.cc"],
    deps = [
        ":file",
        "//cpp/platform/base",
        "//file/util:temp_path",
        "@com_google_googletest//:gtest_main","@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_absl//absl/strings",
    ],
)
