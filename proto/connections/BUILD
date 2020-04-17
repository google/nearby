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

load("//net/proto2/contrib/portable/cc:portable_proto_build_defs.bzl", "portable_proto_library")

proto_library(
    name = "offline_wire_formats_proto",
    srcs = [
        "offline_wire_formats.proto",
    ],
    cc_api_version = 2,
    visibility = ["//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__"],
)

java_lite_proto_library(
    name = "offline_wire_formats_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby:__subpackages__",
    ],
    deps = [":offline_wire_formats_proto"],
)

portable_proto_library(
    name = "offline_wire_formats_portable_proto",
    config = ":offline_wire_formats_proto_config",
    copts = [
        "-DGOOGLE_PROTOBUF_NO_RTTI=1",
    ],
    header_outs = [
        "offline_wire_formats.pb.h",
    ],
    proto_deps = [
        ":offline_wire_formats_proto",
    ],
    visibility = ["//location/nearby/connections:__subpackages__"],
)

filegroup(
    name = "offline_wire_formats_proto_config",
    srcs = ["offline_wire_formats_proto_config.asciipb"],
)
