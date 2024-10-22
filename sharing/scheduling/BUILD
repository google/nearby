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
    name = "format",
    srcs = ["format.cc"],
    hdrs = ["format.h"],
    deps = [
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "scheduling",
    srcs = [
        "nearby_share_expiration_scheduler.cc",
        "nearby_share_on_demand_scheduler.cc",
        "nearby_share_periodic_scheduler.cc",
        "nearby_share_scheduler.cc",
        "nearby_share_scheduler_base.cc",
        "nearby_share_scheduler_factory.cc",
        "nearby_share_scheduler_fields.h",
        "nearby_share_scheduler_utils.cc",
    ],
    hdrs = [
        "nearby_share_expiration_scheduler.h",
        "nearby_share_on_demand_scheduler.h",
        "nearby_share_periodic_scheduler.h",
        "nearby_share_scheduler.h",
        "nearby_share_scheduler_base.h",
        "nearby_share_scheduler_factory.h",
        "nearby_share_scheduler_utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":format",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:types",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "test_support",
    testonly = True,
    srcs = [
        "fake_nearby_share_scheduler.cc",
        "fake_nearby_share_scheduler_factory.cc",
    ],
    hdrs = [
        "fake_nearby_share_scheduler.h",
        "fake_nearby_share_scheduler_factory.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":scheduling",
        "//internal/platform:types",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "scheduling_test",
    srcs = [
        "nearby_share_expiration_scheduler_test.cc",
        "nearby_share_on_demand_scheduler_test.cc",
        "nearby_share_periodic_scheduler_test.cc",
        "nearby_share_scheduler_base_test.cc",
        "nearby_share_scheduler_fields.h",
        "nearby_share_scheduler_utils_test.cc",
    ],
    deps = [
        ":scheduling",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:types",
        "//sharing/internal/test:nearby_test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
