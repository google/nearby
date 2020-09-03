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
    name = "utils",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    visibility = [
        "//core/internal/mediums/webrtc:__pkg__",
    ],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
        "//absl/strings",
    ],
)

cc_library(
    name = "mediums",
    srcs = [
        "ble_advertisement.cc",
        "ble_advertisement_header.cc",
        "ble_packet.cc",
        "ble_peripheral.cc",
    ],
    hdrs = [
        "advertisement_read_result.cc",
        "advertisement_read_result.h",
        "ble.cc",
        "ble.h",
        "ble_advertisement.h",
        "ble_advertisement_header.h",
        "ble_packet.h",
        "ble_peripheral.h",
        "ble_v2.cc",
        "ble_v2.h",
        "bloom_filter.cc",
        "bloom_filter.h",
        "bluetooth_classic.cc",
        "bluetooth_classic.h",
        "bluetooth_radio.cc",
        "bluetooth_radio.h",
        "discovered_peripheral_callback.h",
        "discovered_peripheral_tracker.cc",
        "discovered_peripheral_tracker.h",
        "lost_entity_tracker.cc",
        "lost_entity_tracker.h",
        "mediums.cc",
        "mediums.h",
        "uuid.cc",
        "uuid.h",
        "wifi_lan.cc",
        "wifi_lan.h",
    ],
    visibility = ["//core/internal:__pkg__"],
    deps = [
        ":utils",
        "//platform:logging",
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
        "//absl/numeric:int128",
        "//absl/strings",
        "//smhasher:libmurmur3",
    ],
)

cc_test(
    name = "advertisement_read_result_test",
    srcs = ["advertisement_read_result_test.cc"],
    deps = [
        ":mediums",
        "//platform/api",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
        "//absl/time",
    ],
)

cc_test(
    name = "ble_advertisement_header_test",
    srcs = ["ble_advertisement_header_test.cc"],
    deps = [
        ":mediums",
        "//platform:utils",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "ble_advertisement_test",
    srcs = ["ble_advertisement_test.cc"],
    deps = [
        ":mediums",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "ble_packet_test",
    srcs = ["ble_packet_test.cc"],
    deps = [
        ":mediums",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "bloom_filter_test",
    srcs = ["bloom_filter_test.cc"],
    deps = [
        ":mediums",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "lost_entity_tracker_test",
    srcs = ["lost_entity_tracker_test.cc"],
    deps = [
        ":mediums",
        "//platform/api",
        "//platform/impl/g3",
        "//testing/base/public:gunit_main",
    ],
)
