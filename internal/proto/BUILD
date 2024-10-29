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

# NOTE: gRPC libraries not needed.
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "credential_proto",
    srcs = ["credential.proto"],
    visibility = [
        "//location/nearby/presence/proto:__pkg__",
        "//location/nearby/sharing/proto:__pkg__",
    ],
)

cc_proto_library(
    name = "credential_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":credential_proto"],
)

proto_library(
    name = "local_credential_proto",
    srcs = ["local_credential.proto"],
    visibility = [
        "//location/nearby/presence/proto:__pkg__",
        "//location/nearby/sharing/proto:__pkg__",
    ],
    deps = [":credential_proto"],
)

cc_proto_library(
    name = "local_credential_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":local_credential_proto"],
)

proto_library(
    name = "metadata_proto",
    srcs = ["metadata.proto"],
    visibility = ["//visibility:private"],
)

proto_library(
    name = "tachyon_proto",
    srcs = [
        "duration.proto",
        "ice.proto",
        "messaging.proto",
        "tachyon.proto",
        "tachyon_common.proto",
        "tachyon_enums.proto",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        "//frameworks/testing/rpcreplay/processors:rpc_replay_field_option_proto",
        "//google/api:annotations",
    ],
)

cc_proto_library(
    name = "metadata_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":metadata_proto"],
)

cc_proto_library(
    name = "tachyon_cc_proto",
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:public"],
    deps = [":tachyon_proto"],
)
