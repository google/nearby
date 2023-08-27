# Copyright 2023 Google LLC
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
    name = "retroactive",
    srcs = [
        "retroactive.cc",
        "retroactive_pairing_detector_impl.cc",
    ],
    hdrs = [
        "retroactive.h",
        "retroactive_pairing_detector.h",
        "retroactive_pairing_detector_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair:fast_pair_controller",
        "//fastpair/common",
        "//fastpair/internal/mediums",
        "//fastpair/message_stream",
        "//fastpair/pairing",
        "//fastpair/repository",
        "//fastpair/repository:device_repository",
        "//internal/account",
        "//internal/base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//third_party/magic_enum",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "retroactive_test",
    size = "small",
    srcs = [
        "retroactive_test.cc",
    ],
    deps = [
        ":retroactive",
        "//fastpair/common",
        "//fastpair/message_stream:fake_gatt_callbacks",
        "//fastpair/message_stream:fake_provider",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository:test_support",
        "//internal/base:bluetooth_address",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
