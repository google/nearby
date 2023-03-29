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
    name = "fastpair_wrapper",
    srcs = [
        "fast_pair_wrapper.cc",
        "fast_pair_wrapper_impl.cc",
    ],
    hdrs = [
        "fast_pair_wrapper.h",
        "fast_pair_wrapper_impl.h",
    ],
    copts = ["-Ithird_party"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/scanning:scanner",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
    ],
)

cc_test(
    name = "fastpair_wrapper_test",
    size = "small",
    timeout = "short",
    srcs = [
        "fast_pair_wrapper_impl_test.cc",
        "fast_pair_wrapper_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    shard_count = 8,
    deps = [
        ":fastpair_wrapper",
        "//internal/platform:test_util",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
