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

package(default_visibility = [
    "//:__subpackages__",
])

licenses(["notice"])

cc_library(
    name = "common",
    srcs = ["data_element.cc"],
    hdrs = ["data_element.h"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "dct_advertisement",
    srcs = ["dct_advertisement.cc"],
    hdrs = ["dct_advertisement.h"],
    deps = [
        ":common",
        "//internal/crypto_cros",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform:util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "util",
    srcs = ["advertisement_util.cc"],
    hdrs = ["advertisement_util.h"],
    deps = [
        ":dct_advertisement",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:util",
    ],
)

cc_test(
    name = "data_element_test",
    srcs = ["data_element_test.cc"],
    deps = [
        ":common",
        "//internal/platform:base",
        "//internal/platform:util",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "dct_advertisement_test",
    srcs = ["dct_advertisement_test.cc"],
    deps = [
        ":dct_advertisement",
        "//internal/platform:base",
        "//internal/platform:util",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "advertisement_util_test",
    srcs = ["advertisement_util_test.cc"],
    deps = [
        ":util",
        "//internal/platform:base",
        "//internal/platform:util",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
