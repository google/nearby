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

proto_library(
    name = "share_proto",
    srcs = [
        "certificate_rpc.proto",
        "contact_rpc.proto",
        "device_rpc.proto",
        "encrypted_metadata.proto",
        "field_mask.proto",
        "rpc_resources.proto",
        "settings_observer_data.proto",
        "timestamp.proto",
    ],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "share_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":share_proto"],
)

proto_library(
    name = "wire_format_proto",
    srcs = ["wire_format.proto"],
    visibility = ["//visibility:public"],
    deps = ["//proto:sharing_enums_proto"],
)

cc_proto_library(
    name = "wire_format_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":wire_format_proto"],
)

proto_library(
    name = "enums_proto",
    srcs = ["enums.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "enums_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":enums_proto"],
)
