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

# Proto for Nearby products

# Placeholder: load py_proto_library

load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "connections_enums_proto",
    srcs = ["connections_enums.proto"],
)

java_proto_library(
    name = "connections_enums_java_proto",
    deps = [":connections_enums_proto"],
)

cc_proto_library(
    name = "connections_enums_cc_proto",
    deps = [":connections_enums_proto"],
)

proto_library(
    name = "sharing_enums_proto",
    srcs = ["sharing_enums.proto"],
)

proto_library(
    name = "fast_pair_enums_proto",
    srcs = ["fast_pair_enums.proto"],
    compatible_with = ["//buildenv/target:non_prod"],
)

cc_proto_library(
    name = "fast_pair_enums_cc_proto",
    deps = [":fast_pair_enums_proto"],
)

java_proto_library(
    name = "fast_pair_enums_java_proto",
    deps = [":fast_pair_enums_proto"],
)

cc_proto_library(
    name = "sharing_enums_cc_proto",
    deps = [":sharing_enums_proto"],
)

java_proto_library(
    name = "sharing_enums_java_proto",
    deps = [":sharing_enums_proto"],
)
