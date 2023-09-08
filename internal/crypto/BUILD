licenses(["notice"])

# Copyright 2020 Google LLC
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
package(default_visibility = [
    "//:__subpackages__",
    "//location/nearby/cpp/sharing/implementation:__subpackages__",
])

licenses(["notice"])

cc_library(
    name = "crypto",
    srcs = ["ed25519.cc"],
    hdrs = ["ed25519.h"],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        "//internal/crypto_cros",
        "@boringssl//:crypto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "crypto_unittests",
    size = "small",
    srcs = ["ed25519_unittest.cc"],
    copts = [
        "-DUNIT_TEST",
        "-Wno-inconsistent-missing-override",
        "-Wno-non-virtual-dtor",
        "-Ithird_party",
    ],
    deps = [
        ":crypto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
