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
        "//storage/datapol/annotations/proto:datapol_annotations",
        "//proto:connections_enums_proto",
        "//third_party/nearby_connections/proto/errorcode:error_code_enums_proto",
    ],
)

cc_proto_library(
    name = "connections_log_cc_proto",
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//third_party/nearby_connections:__subpackages__",
    ],
    deps = [":connections_log_proto"],
)

java_lite_proto_library(
    name = "connections_log_java_proto_lite",
    deps = [":connections_log_proto"],
)
