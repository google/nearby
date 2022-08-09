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

package(default_visibility = ["//third_party/nearby:__subpackages__"])

cc_library(
    name = "presence",
    srcs = [
        "presence_client.cc",
    ],
    hdrs = [
        "presence_client.h",
        "presence_service.h",
    ],
    deps = [
        ":types",
        "//presence/implementation:internal",  # build_cleaner: keep
    ],
)

cc_library(
    name = "types",
    srcs = [
        "device_motion.cc",
        "discovery_filter.cc",
        "presence_action.cc",
        "presence_client.cc",
        "presence_device.cc",
        "presence_zone.cc",
        "scan_request_builder.cc",
    ],
    hdrs = [
        "broadcast_options.h",
        "data_element.h",
        "device_motion.h",
        "discovery_filter.h",
        "discovery_options.h",
        "listeners.h",
        "power_mode.h",
        "presence_action.h",
        "presence_client.h",
        "presence_device.h",
        "presence_zone.h",
        "scan_request.h",
        "scan_request_builder.h",
        "status.h",
    ],
    deps = [
        ":credential",
        "//net/proto2/util/public:differencer",
        "//internal/platform:base",
        "//presence/proto:credential_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_glog//:glog",
    ],
)

cc_library(
    name = "broadcast_request",
    srcs = ["broadcast_request.cc"],
    hdrs = ["broadcast_request.h"],
    deps = [
        ":credential",
        ":encryption",
        ":types",
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "encryption",
    srcs = ["encryption.cc"],
    hdrs = ["encryption.h"],
    deps = [
        "//internal/platform:logging",
        "//third_party/tink/cc/subtle",
        "@boringssl//:crypto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "certificate_manager",
    hdrs = ["certificate_manager.h"],
    deps = [
        ":credential",
        ":types",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "advertisement_factory",
    srcs = ["advertisement_factory.cc"],
    hdrs = ["advertisement_factory.h"],
    deps = [
        ":broadcast_request",
        ":certificate_manager",
        ":credential",
        ":types",
        "//internal/platform:logging",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "action_factory",
    srcs = ["action_factory.cc"],
    hdrs = ["action_factory.h"],
    deps = [
        ":broadcast_request",
        ":types",
        "//internal/platform:logging",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "credential",
    srcs = ["presence_identity.cc"],
    hdrs = [
        "presence_identity.h",
    ],
    visibility = [
        "//third_party/nearby:__subpackages__",
    ],
    deps = [
        "//presence/proto:credential_cc_proto",
        "//presence/proto:device_metadata_cc_proto",
    ],
)

cc_test(
    name = "advertisement_factory_test",
    size = "small",
    srcs = ["advertisement_factory_test.cc"],
    deps = [
        ":action_factory",
        ":advertisement_factory",
        ":certificate_manager",
        ":credential",
        ":types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "broadcast_request_test",
    size = "small",
    srcs = ["broadcast_request_test.cc"],
    deps = [
        ":broadcast_request",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/types:variant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encryption_test",
    size = "small",
    srcs = ["encryption_test.cc"],
    deps = [
        ":encryption",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "action_factory_test",
    size = "small",
    srcs = ["action_factory_test.cc"],
    deps = [
        ":action_factory",
        ":broadcast_request",
        ":types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
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
        "status_test.cc",
    ],
    shard_count = 6,
    deps = [
        ":credential",
        ":types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
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
        ":credential",
        "//internal/platform:uuid",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//presence/proto:credential_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
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
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
