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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "ble_advertisement_header",
    srcs = ["ble_advertisement_header.cc"],
    hdrs = ["ble_advertisement_header.h"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal/platform/implementation/windows:__pkg__",
    ],
    deps = [
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:util",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "bloom_filter",
    srcs = ["bloom_filter.cc"],
    hdrs = ["bloom_filter.h"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal/platform/implementation/windows:__pkg__",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "@aappleby_smhasher//:libmurmur3",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "ble_socket",
    srcs = ["ble_socket.cc"],
    hdrs = ["ble_socket.h"],
    visibility = [
        "//connections/implementation:__subpackages__",
        "//internal/platform/implementation/windows:__pkg__",
    ],
    deps = [
        ":ble",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform:util",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "ble_socket_test",
    srcs = ["ble_socket_test.cc"],
    deps = [
        ":ble",
        ":ble_socket",
        "//connections/implementation/flags:connections_flags",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:util",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/g3",  # buildcleaner: keep
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ble",
    srcs = [
        "advertisement_read_result.cc",
        "ble_advertisement.cc",
        "ble_l2cap_packet.cc",
        "ble_packet.cc",
        "ble_utils.cc",
        "discovered_peripheral_tracker.cc",
        "instant_on_lost_advertisement.cc",
        "instant_on_lost_manager.cc",
    ],
    hdrs = [
        "advertisement_read_result.h",
        "ble_advertisement.h",
        "ble_l2cap_packet.h",
        "ble_packet.h",
        "ble_utils.h",
        "discovered_peripheral_callback.h",
        "discovered_peripheral_tracker.h",
        "instant_on_lost_advertisement.h",
        "instant_on_lost_manager.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        ":ble_advertisement_header",
        ":bloom_filter",
        "//connections/implementation:ble_advertisement",
        "//connections/implementation:types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums:lost_entity_tracker",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/mediums/advertisements:dct_advertisement",
        "//connections/implementation/mediums/advertisements:util",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform:util",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//proto/mediums:ble_frames_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "ble_test",
    srcs = [
        "advertisement_read_result_test.cc",
        "ble_advertisement_header_test.cc",
        "ble_advertisement_test.cc",
        "ble_l2cap_packet_test.cc",
        "ble_packet_test.cc",
        "ble_utils_test.cc",
        "bloom_filter_test.cc",
        "discovered_peripheral_tracker_test.cc",
        "instant_on_lost_advertisement_test.cc",
        "instant_on_lost_manager_test.cc",
    ],
    deps = [
        ":ble",
        ":ble_advertisement_header",
        ":bloom_filter",
        "//connections/implementation:types",
        "//connections/implementation/flags:connections_flags",
        "//connections/implementation/mediums:utils",
        "//connections/implementation/mediums/advertisements:dct_advertisement",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:mac_address",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation/g3",  # buildcleaner: keep
        "//internal/test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
