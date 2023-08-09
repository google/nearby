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
licenses(["notice"])

cc_library(
    name = "core",
    srcs = [
        "core.cc",
    ],
    hdrs = [
        "core.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//connections:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/testing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        ":core_types",
        "//connections/implementation:internal",
        "//connections/v3:v3_types",
        "//internal/analytics:event_logger",
        "//internal/interop:device",
        "//internal/platform:base",
        "//internal/platform:types",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "core_types",
    srcs = [
        "advertising_options.cc",
        "connection_options.cc",
        "discovery_options.cc",
        "payload.cc",
        "status.cc",
        "strategy.cc",
    ],
    hdrs = [
        "advertising_options.h",
        "connection_options.h",
        "discovery_options.h",
        "listeners.h",
        "medium_selector.h",
        "options_base.h",
        "out_of_band_connection_metadata.h",
        "params.h",
        "payload.h",
        "payload_type.h",
        "power_level.h",
        "status.h",
        "strategy.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//connections:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/testing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform:util",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "core_test",
    size = "small",
    srcs = [
        "core_test.cc",
        "listeners_test.cc",
        "payload_test.cc",
        "status_test.cc",
        "strategy_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":core",
        ":core_types",
        "//connections/implementation:internal_test",
        "//connections/v3:v3_types",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
