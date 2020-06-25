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
load("//tools/build_defs/proto/cpp:cc_proto_library.bzl", "cc_proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "nfc_frames_proto",
    srcs = [
        "nfc_frames.proto",
    ],
    cc_api_version = 2,
)

java_lite_proto_library(
    name = "nfc_frames_java_proto_lite",
    visibility = ["//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__"],
    deps = [":nfc_frames_proto"],
)

proto_library(
    name = "wifi_aware_frames_proto",
    srcs = [
        "wifi_aware_frames.proto",
    ],
    cc_api_version = 2,
)

java_lite_proto_library(
    name = "wifi_aware_frames_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__",
    ],
    deps = [":wifi_aware_frames_proto"],
)

proto_library(
    name = "ble_frames_proto",
    srcs = [
        "ble_frames.proto",
    ],
    cc_api_version = 2,
)

java_lite_proto_library(
    name = "ble_frames_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__",
    ],
    deps = [":ble_frames_proto"],
)

proto_library(
    name = "web_rtc_signaling_frames_proto",
    srcs = [
        "web_rtc_signaling_frames.proto",
    ],
    cc_api_version = 2,
)

cc_proto_library(
    name = "web_rtc_signaling_frames_cc_proto",
    visibility = ["//location/nearby/connections:__subpackages__"],
    deps = [":web_rtc_signaling_frames_proto"],
)

java_lite_proto_library(
    name = "web_rtc_signaling_frames_java_proto_lite",
    strict_deps = 0,
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/mediums:__subpackages__",
    ],
    deps = [":web_rtc_signaling_frames_proto"],
)

portable_proto_library(
    name = "ble_frames_portable_proto",
    config = ":ble_frames_portable_proto_config",
    copts = [
        "-DGOOGLE_PROTOBUF_NO_RTTI=1",
    ],
    header_outs = [
        "ble_frames.pb.h",
    ],
    proto_deps = [
        ":ble_frames_proto",
    ],
)

filegroup(
    name = "ble_frames_portable_proto_config",
    srcs = ["ble_frames_portable_proto_config.asciipb"],
)
