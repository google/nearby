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
    name = "keyed_service",
    srcs = [
        "fast_pair_mediator.cc",
    ],
    hdrs = [
        "fast_pair_mediator.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/scanning:scanner",
        "//fastpair/server_access",
        "//internal/platform:logging",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "fast_pair_mediator_test",
    size = "small",
    srcs = [
        "fast_pair_mediator_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":keyed_service",
        "//fastpair/scanning:mocks",
        "//fastpair/server_access:mocks",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
