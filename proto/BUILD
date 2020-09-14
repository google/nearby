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
load("//tools/build_defs/proto/cpp:cc_proto_library.bzl", "cc_proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "bootstrap_enums_proto",
    srcs = ["bootstrap_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = ["//logs/proto/logs_annotations"],
)

java_lite_proto_library(
    name = "bootstrap_enums_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby:nearby_packages",
    ],
    deps = [":bootstrap_enums_proto"],
)

proto_library(
    name = "discovery_enums_proto",
    srcs = ["discovery_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = ["//logs/proto/logs_annotations"],
)

java_lite_proto_library(
    name = "discovery_enums_java_proto_lite",
    deps = [":discovery_enums_proto"],
)

java_proto_library(
    name = "discovery_enums_java_proto",
    compatible_with = ["//buildenv/target:appengine"],
    deps = [":discovery_enums_proto"],
)

proto_library(
    name = "error_code_enums_proto",
    srcs = ["error_code_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "error_code_enums_java_proto_lite",
    deps = [":error_code_enums_proto"],
)

proto_library(
    name = "connections_enums_proto",
    srcs = ["connections_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "connections_enums_java_proto_lite",
    deps = [":connections_enums_proto"],
)

go_proto_library(
    name = "connections_enums_go_proto",
    deps = [":connections_enums_proto"],
)

java_proto_library(
    name = "connections_enums_java_proto",
    deps = [":connections_enums_proto"],
)

portable_proto_library(
    name = "connections_enums_portable_proto",
    config = ":connections_enums_proto_config",
    copts = [
        "-DGOOGLE_PROTOBUF_NO_RTTI=1",
    ],
    header_outs = [
        "connections_enums.pb.h",
    ],
    proto_deps = [
        ":connections_enums_proto",
    ],
    visibility = ["//location/nearby/connections:__subpackages__"],
)

filegroup(
    name = "connections_enums_proto_config",
    srcs = ["connections_enums_proto_config.asciipb"],
)

proto_library(
    name = "setup_enums_proto",
    srcs = ["setup_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "setup_enums_java_proto_lite",
    deps = [":setup_enums_proto"],
)

proto_library(
    name = "nearby_client_enums_proto",
    srcs = ["nearby_client_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "nearby_client_enums_java_proto_lite",
    deps = [":nearby_client_enums_proto"],
)

go_proto_library(
    name = "nearby_client_enums_go_proto",
    deps = [":nearby_client_enums_proto"],
)

proto_library(
    name = "magic_pair_enums_proto",
    srcs = ["magic_pair_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

cc_proto_library(
    name = "magic_pair_enums_cc_proto",
    compatible_with = ["//buildenv/target:appengine"],
    deps = [":magic_pair_enums_proto"],
)

java_lite_proto_library(
    name = "magic_pair_enums_java_proto_lite",
    deps = [":magic_pair_enums_proto"],
)

go_proto_library(
    name = "magic_pair_enums_go_proto",
    deps = [":magic_pair_enums_proto"],
)

proto_library(
    name = "sharing_enums_proto",
    srcs = ["sharing_enums.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "sharing_enums_java_proto_lite",
    deps = [":sharing_enums_proto"],
)

java_proto_library(
    name = "sharing_enums_java_proto",
    deps = [":sharing_enums_proto"],
)

proto_library(
    name = "nearby_event_codes_proto",
    srcs = ["nearby_event_codes.proto"],
    cc_api_version = 2,
    compatible_with = ["//buildenv/target:appengine"],
    deps = [
        "//logs/proto/logs_annotations",
    ],
)

java_lite_proto_library(
    name = "nearby_event_codes_java_proto_lite",
    deps = [":nearby_event_codes_proto"],
)

go_proto_library(
    name = "nearby_event_codes_go_proto",
    deps = [":nearby_event_codes_proto"],
)
