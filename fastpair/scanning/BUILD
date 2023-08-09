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
    name = "scanner",
    srcs = [
        "scanner_broker_impl.cc",
    ],
    hdrs = [
        "scanner_broker.h",
        "scanner_broker_impl.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/internal/mediums",
        "//fastpair/repository:device_repository",
        "//fastpair/scanning/fastpair:scanning",
        "//internal/base",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:bind_front",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = [
        "mock_scanner_broker.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":scanner",
        "//fastpair/common",
        "//internal/base",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "scanner_broker_impl_test",
    size = "small",
    srcs = [
        "scanner_broker_impl_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":scanner",
        "//fastpair/common",
        "//fastpair/internal/mediums",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository:device_repository",
        "//fastpair/repository:test_support",
        "//fastpair/testing",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
