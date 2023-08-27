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
        "pairer_broker_impl.cc",
    ],
    hdrs = [
        "pairer_broker.h",
        "pairer_broker_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//fastpair:__subpackages__",
        "//internal:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/handshake",
        "//fastpair/internal/mediums",
        "//fastpair/pairing/fastpair:pairing",
        "//internal/account",
        "//internal/base",
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "pairer_broker_impl_test",
    size = "small",
    srcs = [
        "pairer_broker_impl_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":pairing",
        "//fastpair/common",
        "//fastpair/crypto",
        "//fastpair/handshake",
        "//fastpair/handshake:test_support",
        "//fastpair/internal/mediums",
        "//fastpair/pairing/fastpair:pairing",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository:test_support",
        "//internal/account",
        "//internal/account:test_support",
        "//internal/auth:credential",
        "//internal/base:bluetooth_address",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test/google3_only:test",
        "@boringssl//:crypto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_googletest//:gtest_main",
    ],
)
