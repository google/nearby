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
    name = "internal",
    srcs = [
        "action_factory.cc",
        "advertisement_decoder.cc",
        "advertisement_factory.cc",
        "base_broadcast_request.cc",
        "credential_manager_impl.cc",
        "encryption.cc",
    ],
    hdrs = [
        "action_factory.h",
        "advertisement_decoder.h",
        "advertisement_factory.h",
        "base_broadcast_request.h",
        "broadcast_manager.h",
        "credential_manager.h",
        "credential_manager_impl.h",
        "encryption.h",
        "mock_service_controller.h",
        "scan_manager.h",
        "service_controller.h",
        "service_controller_impl.h",
    ],
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        "//internal/crypto",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/proto:credential_cc_proto",
        "//presence:types",
        "//presence/implementation/mediums",
        "//third_party/tink/cc/subtle",
        "@boringssl//:crypto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "sensor_fusion",
    hdrs = ["sensor_fusion.h"],
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        "//presence:types",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "advertisement_decoder_test",
    size = "small",
    srcs = ["advertisement_decoder_test.cc"],
    deps = [
        ":internal",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "advertisement_factory_test",
    size = "small",
    srcs = ["advertisement_factory_test.cc"],
    deps = [
        ":internal",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encryption_test",
    size = "small",
    srcs = ["encryption_test.cc"],
    deps = [
        ":internal",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "base_broadcast_request_test",
    srcs = ["base_broadcast_request_test.cc"],
    deps = [
        ":internal",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/types:variant",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "action_factory_test",
    size = "small",
    srcs = ["action_factory_test.cc"],
    deps = [
        ":internal",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "credential_manager_impl_test",
    size = "small",
    srcs = ["credential_manager_impl_test.cc"],
    deps = [
        ":internal",
        "//net/proto2/contrib/parse_proto:testing",
        "//internal/platform:comm",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/proto:credential_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
