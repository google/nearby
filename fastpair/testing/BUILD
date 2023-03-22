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

cc_library(
    name = "testing",
    srcs = [
        "fake_context.cc",
        "fast_pair_service_data_creator.cc",
    ],
    hdrs = [
        "fake_context.h",
        "fast_pair_service_data_creator.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//internal/test",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "fake_context_test",
    size = "small",
    srcs = [
        "fake_context_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":testing",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
