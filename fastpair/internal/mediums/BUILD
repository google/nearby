# Copyright 2022 Google LLC
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
        "ble_v2.cc",
        "bluetooth_classic.cc",
        "bluetooth_radio.cc",
        "robust_gatt_client.cc",
    ],
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_classic.h",
        "bluetooth_radio.h",
        "mediums.h",
        "robust_gatt_client.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "bluetooth_radio_test",
    size = "small",
    srcs = [
        "bluetooth_radio_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mediums_test",
    size = "small",
    srcs = [
        "mediums_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ble_test",
    size = "small",
    srcs = [
        "ble_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ble_v2_test",
    size = "small",
    srcs = [
        "ble_v2_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "bluetooth_classic_test",
    size = "small",
    srcs = [
        "bluetooth_classic_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "robust_gatt_client_test",
    size = "small",
    srcs = [
        "robust_gatt_client_test.cc",
    ],
    deps = [
        ":mediums",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
