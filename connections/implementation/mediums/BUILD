load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

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
licenses(["notice"])

cc_library(
    name = "mediums",
    srcs = [
        "awdl.cc",
        "ble.cc",
        "ble_v2.cc",
        "bluetooth_classic.cc",
        "bluetooth_radio.cc",
        "mediums.cc",
        "webrtc.cc",
        "webrtc_stub.cc",
        "wifi_direct.cc",
        "wifi_hotspot.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "awdl.h",
        "ble.h",
        "ble_v2.h",
        "bluetooth_classic.h",
        "bluetooth_radio.h",
        "mediums.h",
        "webrtc.h",
        "webrtc_stub.h",
        "wifi.h",
        "wifi_direct.h",
        "wifi_hotspot.h",
        "wifi_lan.h",
    ],
    copts = ["-DNO_WEBRTC"],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        ":utils",
        ":webrtc_utils",
        "//connections:core_types",
        "//connections/implementation:types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums/ble_v2",
        "//connections/implementation/mediums/ble_v2:ble_advertisement_header",
        "//connections/implementation/mediums/ble_v2:bloom_filter",
        "//connections/implementation/mediums/multiplex",
        "//connections/implementation/mediums/webrtc",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:wifi_utils",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        # TODO: Support WebRTC
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "webrtc_utils",
    srcs = [
        "webrtc_peer_id.cc",
        "webrtc_peer_id_stub.cc",
    ],
    hdrs = [
        "webrtc_peer_id.h",
        "webrtc_peer_id_stub.h",
        "webrtc_socket.h",
        "webrtc_socket_stub.h",
    ],
    visibility = [
        "//connections/implementation:__pkg__",
        "//connections/implementation/mediums:__pkg__",
        "//connections/implementation/mediums/webrtc:__pkg__",
    ],
    deps = [
        ":utils",
        "//connections/implementation/mediums/webrtc:data_types",
        "//internal/platform:base",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "lost_entity_tracker",
    hdrs = ["lost_entity_tracker.h"],
    visibility = [
        "//connections/implementation/mediums/ble_v2:__pkg__",
    ],
    deps = [
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    copts = ["-DNO_WEBRTC"],
    visibility = [
        "//connections/implementation:__pkg__",
        "//connections/implementation/mediums/advertisements:__pkg__",
        "//connections/implementation/mediums/ble_v2:__subpackages__",
        "//connections/implementation/mediums/multiplex:__pkg__",
        "//internal/platform/implementation/windows:__pkg__",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:types",
    ],
)

cc_test(
    name = "core_internal_mediums_test",
    size = "small",
    srcs = [
        "awdl_test.cc",
        "ble_test.cc",
        "ble_v2_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_radio_test.cc",
        "lost_entity_tracker_test.cc",
        "wifi_direct_test.cc",
        "wifi_hotspot_test.cc",
        "wifi_lan_test.cc",
        "wifi_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":lost_entity_tracker",
        ":mediums",
        ":utils",
        "//connections:core_types",
        "//connections/implementation:types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums/ble_v2",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "core_internal_mediums_webrtc_test",
    size = "small",
    srcs = [
        "webrtc_peer_id_test.cc",
        "webrtc_test.cc",
    ],
    shard_count = 16,
    tags = [
        "notsan",  # NOTE(b/139734036): known data race in usrsctplib.
        "requires-net:external",
    ],
    deps = [
        ":mediums",
        ":webrtc_utils",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
