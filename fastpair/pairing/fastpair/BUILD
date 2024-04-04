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
    name = "pairing",
    srcs = [
        "fast_pair_pairer_impl.cc",
    ],
    hdrs = [
        "fast_pair_pairer.h",
        "fast_pair_pairer_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/common",
        "//fastpair/crypto",
        "//fastpair/handshake",
        "//fastpair/internal/mediums",
        "//fastpair/repository",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform/implementation:account_manager",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "fast_pair_pairer_impl_test",
    size = "small",
    srcs = [
        "fast_pair_pairer_impl_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":pairing",
        "//fastpair/common",
        "//fastpair/crypto",
        "//fastpair/handshake",
        "//fastpair/handshake:test_support",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository:test_support",
        "//internal/account",
        "//internal/auth:credential",
        "//internal/base:bluetooth_address",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test",
        "@boringssl//:crypto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
