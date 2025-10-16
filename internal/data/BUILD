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

load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

package(default_visibility = [
    "//visibility:public",
])

cc_library(
    name = "data_manager",
    hdrs = [
        "data_set.h",
        "leveldb_data_set.h",
    ],
    deps = [
        "//internal/base:file_path",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//third_party/leveldb:db",
        "//third_party/leveldb:table",
        "//third_party/leveldb:util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf_lite",
    ] + select({
        "@platforms//os:windows": [
            "//location/nearby/apps/better_together/windows/common:leveldb_env_windows",
        ],
        "//conditions:default": [
        ],
    }),
)

proto_library(
    name = "leveldb_data_set_test_proto",
    srcs = ["leveldb_data_set_test.proto"],
)

cc_proto_library(
    name = "leveldb_data_set_test_cc_proto",
    deps = [":leveldb_data_set_test_proto"],
)

cc_test(
    name = "data_manager_test",
    size = "small",
    timeout = "short",
    srcs = [
        "leveldb_data_set_test.cc",
    ],
    deps = [
        ":data_manager",
        ":leveldb_data_set_test_cc_proto",
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
