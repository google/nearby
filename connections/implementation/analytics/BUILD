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
        "throughput_recorder.cc",
    ],
    hdrs = [
        "analytics_recorder.h",
        "connection_attempt_metadata_params.h",
        "packet_meta_data.h",
        "throughput_recorder.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = ["//connections:__subpackages__"],
    deps = [
        "//connections:core_types",
        "//internal/analytics:event_logger",
        "//internal/platform:error_code_recorder",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/proto/analytics:connections_log_cc_proto",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_test(
    name = "analytics_test",
    size = "small",
    srcs = [
        "analytics_recorder_test.cc",
        "throughput_recorder_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":analytics",
        "//internal/analytics:mock_event_logger",
        "//internal/platform:base",
        "//internal/platform:error_code_recorder",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/proto/analytics:connections_log_cc_proto",
        "//net/proto2/contrib/parse_proto:parse_text_proto",
        "//proto:connections_enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
