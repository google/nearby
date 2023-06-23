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

load("//dart:build_defs.bzl", "dart_proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

proto_library(
    name = "fastpair_callback_proto",
    srcs = [
        "callbacks.proto",
    ],
    deps = [
        "//fastpair/proto:fastpair_proto",
    ],
)

proto_library(
    name = "fastpair_device_proto",
    srcs = [
        "device.proto",
    ],
)

proto_library(
    name = "fastpair_enum_proto",
    srcs = [
        "enum.proto",
    ],
)

proto_library(
    name = "fastpair_proto",
    deps = [
        ":fastpair_callback_proto",
        ":fastpair_device_proto",
        ":fastpair_enum_proto",
    ],
)

cc_proto_library(
    name = "fastpair_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":fastpair_proto"],
)

dart_proto_library(
    name = "fastpair_dart_proto",
    visibility = ["//visibility:public"],
    deps = [":fastpair_proto"],
)
