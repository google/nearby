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

load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

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
        "//internal/platform:types",
        "//third_party/leveldb:db",
        "//third_party/leveldb:table",
        "//third_party/leveldb:util",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf_lite",
    ],
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
    shard_count = 8,
    deps = [
        ":data_manager",
        ":leveldb_data_set_test_cc_proto",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
