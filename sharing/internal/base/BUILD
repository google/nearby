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
    name = "utf_utils",
    hdrs = [
        "utf_string_conversions.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "base",
    srcs = [
        "encode.cc",
        "mime.cc",
    ],
    hdrs = [
        "encode.h",
        "mime.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base:no_destructor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "base_test",
    size = "small",
    timeout = "short",
    srcs = [
        "encode_test.cc",
        "mime_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":base",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
