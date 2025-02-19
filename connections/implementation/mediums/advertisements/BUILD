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
