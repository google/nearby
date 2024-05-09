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

cc_library(
    name = "v3_types",
    srcs = [
        "connections_device.cc",
    ],
    hdrs = [
        "advertising_options.h",
        "bandwidth_info.h",
        "connection_listening_options.h",
        "connection_result.h",
        "connections_device.h",
        "connections_device_provider.h",
        "discovery_options.h",
        "listeners.h",
        "listening_result.h",
        "params.h",
    ],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/interop:authentication_status",
        "//internal/interop:device",
        "//internal/platform:connection_info",
        "//internal/platform:types",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "v3_types_test",
    srcs = [
        "connections_device_provider_test.cc",
        "connections_device_test.cc",
    ],
    deps = [
        ":v3_types",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/platform:connection_info",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
