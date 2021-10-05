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

load("//tools/build_defs/proto/cpp:cc_proto_library.bzl", "cc_proto_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "connections_log_proto",
    srcs = [
        "connections_log.proto",
    ],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        
        "//proto:connections_enums_proto",
        "//proto/errorcode:error_code_enums_proto",
    ],
)

cc_proto_library(
    name = "connections_log_cc_proto",
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//analytics:__subpackages__",
        "//core:__subpackages__",
    ],
    deps = [":connections_log_proto"],
)

java_lite_proto_library(
    name = "connections_log_java_proto_lite",
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
        
        "//net/proto2/public",
        "//testing/base/public:gunit_main",
        "//platform/base",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:logging",
        "//proto:connections_enums_portable_proto",
    ],
)
