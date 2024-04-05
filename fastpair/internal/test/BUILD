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
    name = "nearby_fastpair_test",
    hdrs = [
        "fast_pair_fake_http_client.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/network:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "nearby_fastpair_test_test",
    size = "small",
    timeout = "short",
    srcs = [
        "fast_pair_fake_http_client_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":nearby_fastpair_test",
        "//internal/network:types",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
