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
    name = "connections_flags",
    srcs = [
        "connections_flags.cc",
    ],
    hdrs = [
        "connections_flags.h",
        "nearby_connections_feature_flags.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//internal/platform:__subpackages__",
        "//location/nearby/cpp:__subpackages__",
    ],
    deps = [
        "//internal/flags:flag_reader",
        "//internal/platform:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "connections_flags_test",
    srcs = ["connections_flags_test.cc"],
    deps = [
        ":connections_flags",
        "//internal/flags:flag_reader",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
