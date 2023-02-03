# Copyright 2022 Google LLC
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

package_group(
    name = "nearby_fastpair",
    packages = [
        "//fastpair/...",
    ],
)

cc_library(
    name = "fastpair_controller",
    srcs = [
        "fast_pair_controller.cc",
        "fast_pair_controller_impl.cc",
    ],
    hdrs = [
        "fast_pair_controller.h",
        "fast_pair_controller_impl.h",
    ],
    copts = ["-Ithird_party"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/scanning/fastpair:scanning",
        "//internal/platform:base",
        "//internal/platform:logging",
    ],
)

cc_test(
    name = "fastpair_controller_test",
    size = "small",
    timeout = "short",
    srcs = [
        "fast_pair_controller_impl_test.cc",
        "fast_pair_controller_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    shard_count = 8,
    deps = [
        ":fastpair_controller",
        "//fastpair/scanning/fastpair:scanning",
        "//internal/platform:logging",
        "//internal/platform:test_util",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
