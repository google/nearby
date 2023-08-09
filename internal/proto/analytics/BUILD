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

load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "connections_log_proto",
    srcs = [
        "connections_log.proto",
    ],
    deps = [
        "//proto:connections_enums_proto",
        "//proto/errorcode:error_code_enums_proto",
    ],
)

proto_library(
    name = "fast_pair_log_proto",
    srcs = ["fast_pair_log.proto"],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = ["//proto:fast_pair_enums_proto"],
)

cc_proto_library(
    name = "fast_pair_log_cc_proto",
    deps = [":fast_pair_log_proto"],
)

java_proto_library(
    name = "fast_pair_log_java_proto",
    deps = [":fast_pair_log_proto"],
)

cc_proto_library(
    name = "connections_log_cc_proto",
    visibility = [
        "//connections:__subpackages__",
        "//location/nearby/analytics/cpp:__subpackages__",
    ],
    deps = [":connections_log_proto"],
)

cc_test(
    name = "proto_analytics_test",
    size = "small",
    srcs = [
        "connections_log_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":connections_log_cc_proto",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
