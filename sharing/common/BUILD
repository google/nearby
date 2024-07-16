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
    name = "common",
    srcs = [
        "nearby_share_prefs.cc",
        "nearby_share_switches.cc",
    ],
    hdrs = [
        "nearby_share_prefs.h",
        "nearby_share_profile_info_provider.h",
        "nearby_share_switches.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//sharing/internal/api:platform",
        "//sharing/proto:enums_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "test_support",
    testonly = True,
    srcs = [
        "fake_nearby_share_profile_info_provider.cc",
    ],
    hdrs = [
        "fake_nearby_share_profile_info_provider.h",
        "nearby_share_profile_info_provider.h",
    ],
    visibility = ["//visibility:public"],
    deps = [],
)

cc_library(
    name = "enum",
    hdrs = ["nearby_share_enums.h"],
    visibility = ["//visibility:public"],
    deps = [],
)

cc_library(
    name = "compatible_u8_string",
    hdrs = ["compatible_u8_string.h"],
    visibility = [
        "//location/nearby/apps/better_together:__subpackages__",
        "//location/nearby/sharing/sdk:__subpackages__",
        "//sharing:__subpackages__",
    ],
)
