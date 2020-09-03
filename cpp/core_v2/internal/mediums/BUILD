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
        "//core_v2/internal:__subpackages__",
    ],
    deps = [
        "//core_v2:core_types",
        "//core_v2/internal/mediums/webrtc",
        "//platform_v2/base",
        "//platform_v2/public:comm",
        "//platform_v2/public:logging",
        "//platform_v2/public:types",
        "//location/nearby/mediums/proto:web_rtc_signaling_frames_cc_proto",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
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
        "//core_v2/internal:__pkg__",
        "//core_v2/internal/mediums/ble_v2:__pkg__",
        "//core_v2/internal/mediums/webrtc:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
        "//platform_v2/public:types",
    ],
)

cc_test(
    name = "core_v2_internal_mediums_test",
    size = "small",
    srcs = [
        "ble_test.cc",
        "bloom_filter_test.cc",
        "bluetooth_classic_test.cc",
        "bluetooth_radio_test.cc",
        "lost_entity_tracker_test.cc",
        "uuid_test.cc",
        "webrtc_test.cc",
        "wifi_lan_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//core_v2/internal/mediums/webrtc",
        "//platform_v2/base",
        "//platform_v2/base:test_util",
        "//platform_v2/impl/g3",  # build_cleaner: keep
        "//platform_v2/public:comm",
        "//platform_v2/public:logging",
        "//platform_v2/public:types",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/time",
    ],
)
