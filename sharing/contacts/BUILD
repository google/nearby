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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "contacts_interface",
    hdrs = [
        "nearby_share_contact_manager.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//sharing/proto:share_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "contacts",
    srcs = [
        "nearby_share_contact_manager_impl.cc",
    ],
    hdrs = [
        "nearby_share_contact_manager_impl.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":contacts_interface",
        "//internal/platform:types",
        "//internal/platform/implementation:account_manager",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:types",
        "//sharing/proto:share_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "test_support",
    testonly = True,
    hdrs = [
        "fake_nearby_share_contact_manager.h",
    ],
    visibility = ["//visibility:public"],
    deps = [":contacts_interface"],
)

cc_test(
    name = "contacts_test",
    srcs = [
        "nearby_share_contact_manager_impl_test.cc",
    ],
    deps = [
        ":contacts",
        "//internal/platform/implementation:account_manager",
        "//internal/platform/implementation:platform_impl",
        "//internal/test",
        "//sharing/internal/api:mock_sharing_platform",
        "//sharing/internal/test:nearby_test",
        "//sharing/local_device_data:test_support",
        "//sharing/proto:share_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
