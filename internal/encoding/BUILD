# Copyright 2025 Google LLC
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

package(default_visibility = ["//:__subpackages__"])

cc_library(
    name = "base85",
    srcs = ["base85.cc"],
    hdrs = ["base85.h"],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "base85_test",
    srcs = ["base85_test.cc"],
    deps = [
        ":base85",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
