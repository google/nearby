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

package(default_visibility = ["//:__subpackages__"])

cc_library(
    name = "presence",
    srcs = [
        "presence_client.cc",
        "presence_service.cc",
    ],
    hdrs = [
        "presence_client.h",
        "presence_service.h",
    ],
    deps = [
        ":types",
        "//internal/platform:types",
        "//presence/implementation:internal",  # build_cleaner: keep
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "device_motion.cc",
        "discovery_filter.cc",
        "presence_action.cc",
        "presence_device.cc",
        "presence_zone.cc",
        "scan_request_builder.cc",
    ],
    hdrs = [
        "broadcast_options.h",
        "broadcast_request.h",
        "data_element.h",
        "data_types.h",
        "device_motion.h",
        "discovery_filter.h",
        "discovery_options.h",
        "power_mode.h",
        "presence_action.h",
        "presence_device.h",
        "presence_zone.h",
        "scan_request.h",
        "scan_request_builder.h",
    ],
    deps = [
        "//internal:device",
        "//internal/crypto",
        "//internal/platform:connection_info",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "//internal/proto:credential_cc_proto",
        "//internal/proto:metadata_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "types_test",
    size = "small",
    srcs = [
        "broadcast_options_test.cc",
        "device_motion_test.cc",
        "discovery_filter_test.cc",
        "discovery_options_test.cc",
        "presence_action_test.cc",
        "presence_device_test.cc",
        "presence_identity_test.cc",
        "presence_zone_test.cc",
        "scan_request_builder_test.cc",
    ],
    shard_count = 6,
    deps = [
        ":types",
        "@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "//internal/platform:connection_info",
        "//internal/platform:logging",
        "//internal/proto:credential_cc_proto",
    ] + select({
        "//tools/cc_target_os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "credential_test",
    size = "small",
    srcs = [
        "credential_test.cc",
        "presence_identity_test.cc",
    ],
    shard_count = 6,
    deps = [
        "@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_googletest//:gtest_main",
        "//internal/platform:uuid",
        "//internal/proto:credential_cc_proto",
        "//internal/proto:local_credential_cc_proto",
    ] + select({
        "//tools/cc_target_os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "presence_test",
    size = "small",
    srcs = [
        "presence_client_test.cc",
        "presence_service_test.cc",
    ],
    shard_count = 6,
    deps = [
        ":presence",
        ":types",
        "@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/status",
        "//internal/platform:test_util",
    ] + select({
        "//tools/cc_target_os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)
