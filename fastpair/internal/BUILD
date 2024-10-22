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
    name = "internal",
    srcs = ["fast_pair_seeker_impl.cc"],
    hdrs = [
        "fast_pair_seeker_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair:fast_pair_controller",
        "//fastpair:fast_pair_events",
        "//fastpair:fast_pair_seeker",
        "//fastpair/common",
        "//fastpair/internal/mediums",
        "//fastpair/pairing",
        "//fastpair/repository",
        "//fastpair/repository:device_repository",
        "//fastpair/retroactive",
        "//fastpair/scanning:scanner",
        "//internal/platform:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "fast_pair_seeker_impl_test",
    size = "small",
    srcs = [
        "fast_pair_seeker_impl_test.cc",
    ],
    deps = [
        ":internal",
        "//fastpair:fast_pair_events",
        "//fastpair:fast_pair_seeker",
        "//fastpair/common",
        "//fastpair/message_stream:fake_gatt_callbacks",
        "//fastpair/message_stream:fake_provider",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository",
        "//fastpair/repository:device_repository",
        "//fastpair/repository:test_support",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
