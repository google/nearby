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
    name = "ble_v2",
    srcs = [
        "advertisement_read_result.cc",
        "ble_advertisement.cc",
        "ble_advertisement_header.cc",
        "ble_packet.cc",
        "bloom_filter.cc",
    ],
    hdrs = [
        "advertisement_read_result.h",
        "ble_advertisement.h",
        "ble_advertisement_header.h",
        "ble_packet.h",
        "ble_peripheral.h",
        "bloom_filter.h",
        "discovered_peripheral_callback.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/numeric:int128",
        "//absl/strings",
        "//absl/time",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform:util",
        "//smhasher:libmurmur3",
    ],
)

cc_test(
    name = "ble_v2_test",
    srcs = [
        "advertisement_read_result_test.cc",
        "ble_advertisement_header_test.cc",
        "ble_advertisement_test.cc",
        "ble_packet_test.cc",
        "ble_peripheral_test.cc",
        "bloom_filter_test.cc",
    ],
    deps = [
        ":ble_v2",
        "//testing/base/public:gunit_main",
        "//absl/time",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform/implementation/g3",  # buildcleaner: keep
    ],
)
