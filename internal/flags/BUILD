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
    name = "flag_reader",
    hdrs = [
        "default_flag_reader.h",
        "flag.h",
        "flag_reader.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//location/nearby/cpp/experiments:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
    ],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "nearby_flags",
    srcs = [
        "nearby_flags.cc",
    ],
    hdrs = [
        "nearby_flags.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = [
        "//:__subpackages__",
        "//googlemac/iPhone/Shared/Identity/SmartSetup:__subpackages__",
        "//location/nearby/cpp:__subpackages__",
        "//location/nearby/sharing/sdk:__subpackages__",
    ],
    deps = [
        ":flag_reader",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "nearby_flags_test",
    srcs = ["nearby_flags_test.cc"],
    deps = [
        ":flag_reader",
        ":nearby_flags",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
