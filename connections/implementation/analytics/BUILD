load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])
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

cc_library(
    name = "analytics",
    srcs = [
        "analytics_recorder.cc",
    ],
    hdrs = [
        "advertising_metadata_params.h",
        "analytics_recorder.h",
        "connection_attempt_metadata_params.h",
        "discovery_metadata_params.h",
        "operation_result_with_medium.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = ["//connections:__subpackages__"],
    deps = [
        "//connections:core_types",
        "//internal/platform:error_code_recorder",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "analytics_recorder_impl",
    srcs = [
        "analytics_recorder_impl.cc",
    ],
    hdrs = [
        "analytics_recorder_impl.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//connections/c:__pkg__",
        "//sharing:__pkg__",
    ],
    deps = [
        ":analytics",
        "//connections:core_types",
        "//internal/platform:error_code_recorder",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//location/nearby/analytics/cpp/logging:event_logger",
        "//location/nearby/analytics/cpp/proto:connections_log_cc_proto",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_library(
    name = "mock_analytics_recorder",
    testonly = True,
    hdrs = [
        "mock_analytics_recorder.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//connections:__subpackages__"],
    deps = [
        ":analytics",
        "//connections:core_types",
        "//internal/platform:error_code_recorder",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "analytics_test",
    size = "small",
    srcs = [
        "analytics_recorder_impl_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":analytics",
        ":analytics_recorder_impl",
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform:error_code_recorder",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//location/nearby/analytics/cpp/logging:mock_event_logger",
        "//location/nearby/analytics/cpp/proto:connections_log_cc_proto",
        "//net/proto2/contrib/parse_proto:parse_text_proto",
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
