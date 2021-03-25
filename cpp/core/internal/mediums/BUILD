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

cc_library(
    name = "mediums",
    srcs = [
        "ble.cc",
        "bloom_filter.cc",
        "bluetooth_classic.cc",
        "bluetooth_radio.cc",
        "mediums.cc",
        "uuid.cc",
        "webrtc.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "bloom_filter.h",
        "bluetooth_classic.h",
        "bluetooth_radio.h",
        "lost_entity_tracker.h",
        "mediums.h",
        "uuid.h",
        "webrtc.h",
        "wifi_lan.h",
    ],
    visibility = [
        "//core/internal:__subpackages__",
    ],
    deps = [
        ":utils",
        "//core:core_types",
        "//core/internal/mediums/ble_v2",
        "//core/internal/mediums/webrtc",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform/base",
        "//platform/base:cancellation_flag",
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
        "//location/nearby/mediums/proto:web_rtc_signaling_frames_cc_proto",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/functional:bind_front",
        "//absl/numeric:int128",
        "//absl/strings",
        "//absl/time",
        "//smhasher:libmurmur3",
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api:scoped_refptr",
    ],
)

cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    visibility = [
        "//core/internal:__pkg__",
        "//core/internal/mediums:__pkg__",
        "//core/internal/mediums/webrtc:__pkg__",
    ],
    deps = [
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform/base",
        "//platform/public:types",
    ],
)

cc_test(
    name = "core_internal_mediums_test",
    size = "small",
    srcs = [
        "ble_test.cc",
        "bloom_filter_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_radio_test.cc",
        "lost_entity_tracker_test.cc",
        "uuid_test.cc",
        "wifi_lan_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//platform/base:test_util",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:comm",
        "//platform/public:types",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/time",
    ],
)

cc_test(
    name = "core_internal_mediums_webrtc_test",
    size = "small",
    srcs = [
        "webrtc_test.cc",
    ],
    shard_count = 16,
    tags = ["notsan"],  # NOTE(b/139734036): known data race in usrsctplib.
    deps = [
        ":mediums",
        "//core/internal/mediums/webrtc",
        "//platform/base",
        "//platform/base:test_util",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:types",
        "//testing/base/public:gunit_main",
        "//absl/strings",
    ],
)
