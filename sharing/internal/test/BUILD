# Copyright 2021 Google LLC
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
    name = "nearby_test",
    testonly = True,
    srcs = [
        "fake_context.cc",
    ],
    hdrs = [
        "fake_bluetooth_adapter.h",
        "fake_bluetooth_adapter_observer.h",
        "fake_connectivity_manager.h",
        "fake_context.h",
        "fake_fast_initiation_manager.h",
        "fake_network_monitor.h",
        "fake_shell.h",
        "fake_wifi_adapter.h",
        "fake_wifi_adapter_observer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/base",
        "//internal/base:bluetooth_address",
        "//internal/data:data_manager",
        "//internal/network:types",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/test",
        "//third_party/nearby/sharing/internal/api:platform",
        "//third_party/nearby/sharing/internal/public:logging",
        "//third_party/nearby/sharing/internal/public:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "nearby_test_test",
    size = "small",
    timeout = "short",
    srcs = [
        "fake_bluetooth_adapter_test.cc",
        "fake_connectivity_manager_test.cc",
        "fake_context_test.cc",
        "fake_fast_initiation_manager_test.cc",
        "fake_shell_test.cc",
        "fake_wifi_adapter_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":nearby_test",
        "//internal/auth:credential",
        "//internal/auth:types",
        "//internal/data:data_manager",
        "//internal/network:types",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//third_party/nearby/sharing/internal/api:platform",  # fixdeps: keep
        "//third_party/nearby/sharing/internal/public:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
