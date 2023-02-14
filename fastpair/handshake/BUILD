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
    name = "handshake",
    srcs = [
        "fast_pair_data_encryptor_impl.cc",
    ],
    hdrs = [
        "fast_pair_data_encryptor.h",
        "fast_pair_data_encryptor_impl.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/crypto",
        "//fastpair/dataparser",
        "//fastpair/repository",
        "//fastpair/server_access",
        "//internal/base:bluetooth_address",
        "//internal/platform:base",
        "//internal/platform:logging",
        "@boringssl//:crypto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "fast_pair_data_encryptor_impl_test",
    size = "small",
    srcs = [
        "fast_pair_data_encryptor_impl_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":handshake",
        "//fastpair/common",
        "//fastpair/crypto",
        "//fastpair/dataparser",
        "//fastpair/server_access:test_support",
        "//fastpair/testing",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
