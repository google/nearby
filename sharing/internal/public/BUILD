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
    name = "types",
    hdrs = [
        "connectivity_manager.h",
        "context.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/network:url",
        "//internal/platform:types",
        "//sharing/internal/api:platform",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "nearby_context",
    srcs = [
        "connectivity_manager_impl.cc",
        "context_impl.cc",
    ],
    hdrs = [
        "connectivity_manager_impl.h",
        "context_impl.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":logging",
        ":types",
        "//internal/network:url",
        "//internal/platform:types",
        "//sharing/internal/api:platform",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "nearby_context_test",
    size = "small",
    timeout = "short",
    srcs = [
        "connectivity_manager_impl_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":nearby_context",
        ":types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//sharing/internal/api:mock_sharing_platform",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
    ],
)
