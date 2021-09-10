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

load("//net/proto2/contrib/portable/cc:portable_proto_build_defs.bzl", "portable_proto_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "error_code_enums_proto",
    srcs = ["error_code_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "error_code_enums_java_proto_lite",
    deps = [":error_code_enums_proto"],
)

portable_proto_library(
    name = "error_code_enums_portable_proto",
    compatible_with = [
        "//buildenv/target:non_prod",
    ],
    config = ":error_code_enums_proto_config",
    copts = [
        "-DGOOGLE_PROTOBUF_NO_RTTI=1",
    ],
    proto_deps = [
        ":error_code_enums_proto",
    ],
    visibility = [
        "//third_party/nearby_connections:__subpackages__",
    ],
)

filegroup(
    name = "error_code_enums_proto_config",
    srcs = ["error_code_enums_proto_config.asciipb"],
    compatible_with = [
        "//buildenv/target:non_prod",
    ],
)
