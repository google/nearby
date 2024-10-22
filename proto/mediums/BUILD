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

load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "multiplex_frames_proto",
    srcs = [
        "multiplex_frames.proto",
    ],
)

cc_proto_library(
    name = "multiplex_frames_cc_proto",
    visibility = ["//:__subpackages__"],
    deps = [":multiplex_frames_proto"],
)

proto_library(
    name = "nfc_frames_proto",
    srcs = [
        "nfc_frames.proto",
    ],
)

proto_library(
    name = "wifi_aware_frames_proto",
    srcs = [
        "wifi_aware_frames.proto",
    ],
)

proto_library(
    name = "web_rtc_signaling_frames_proto",
    srcs = [
        "web_rtc_signaling_frames.proto",
    ],
)

cc_proto_library(
    name = "web_rtc_signaling_frames_cc_proto",
    visibility = ["//:__subpackages__"],
    deps = [":web_rtc_signaling_frames_proto"],
)

proto_library(
    name = "ble_frames_proto",
    srcs = [
        "ble_frames.proto",
    ],
)

cc_proto_library(
    name = "ble_frames_cc_proto",
    deps = [
        ":ble_frames_proto",
    ],
)
