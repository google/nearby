load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

# Copyright 2024 Google LLC
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
    name = "multiplex",
    srcs = [
        "multiplex_frames.cc",
        "multiplex_output_stream.cc",
        "multiplex_socket.cc",
    ],
    hdrs = [
        "multiplex_frames.h",
        "multiplex_output_stream.h",
        "multiplex_socket.h",
    ],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//connections/implementation/mediums:utils",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//proto:connections_enums_cc_proto",
        "//proto/mediums:multiplex_frames_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "multiplex_test",
    srcs = [
        "multiplex_frames_test.cc",
        "multiplex_output_stream_test.cc",
        "multiplex_socket_test.cc",
    ],
    deps = [
        ":multiplex",
        "//connections/implementation:internal",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # buildcleaner: keep
        "//proto:connections_enums_cc_proto",
        "//proto/mediums:multiplex_frames_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
