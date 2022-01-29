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
        "ble.cc",
        "bluetooth_classic.cc",
        "bluetooth_radio.cc",
        "mediums.cc",
        "uuid.cc",
        "webrtc.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "bluetooth_classic.h",
        "bluetooth_radio.h",
        "lost_entity_tracker.h",
        "mediums.h",
        "uuid.h",
        "webrtc.h",
        "wifi_lan.h",
    ],
    visibility = [
        "//cpp/core/internal:__subpackages__",
    ],
    deps = [
        ":utils",
        "//connections/implementation/proto:offline_wire_formats_portable_proto",
        "//cpp/core:core_types",
        "//cpp/core/internal/mediums/ble_v2",
        "//cpp/core/internal/mediums/webrtc",
        "//cpp/core/internal/mediums/webrtc:data_types",
        "//cpp/platform/base",
        "//cpp/platform/base:cancellation_flag",
        "//cpp/platform/public:comm",
        "//cpp/platform/public:logging",
        "//cpp/platform/public:types",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "utils",
    srcs = [
        "utils.cc",
        "webrtc_peer_id.cc",
    ],
    hdrs = [
        "utils.h",
        "webrtc_peer_id.h",
        "webrtc_socket.h",
    ],
    visibility = [
        "//cpp/core/internal:__pkg__",
        "//cpp/core/internal/mediums:__pkg__",
        "//cpp/core/internal/mediums/webrtc:__pkg__",
    ],
    deps = [
        "//connections/implementation/proto:offline_wire_formats_portable_proto",
        "//cpp/core/internal/mediums/webrtc:data_types",
        "//cpp/platform/base",
        "//cpp/platform/public:types",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "core_internal_mediums_test",
    size = "small",
    srcs = [
        "ble_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_radio_test.cc",
        "lost_entity_tracker_test.cc",
        "uuid_test.cc",
        "wifi_lan_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        ":utils",
        "//cpp/platform/base",
        "//cpp/platform/base:test_util",
        "//cpp/platform/impl/g3",  # build_cleaner: keep
        "//cpp/platform/public:comm",
        "//cpp/platform/public:types",
        "@com_google_googletest//:gtest_main","@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
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
        ":utils",
        "//cpp/platform/base",
        "//cpp/platform/base:test_util",
        "//cpp/platform/impl/g3",  # build_cleaner: keep
        "//cpp/platform/public:types",
        "@com_google_googletest//:gtest_main","@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
    ],
)
