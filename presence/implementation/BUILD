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
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@bazel_skylib//lib:selects.bzl", "selects")

licenses(["notice"])

# Command line build option that enables linking with the LDT encryption implementation in Rust.
#
# When not set, LDT encryption will not be available. We won't be able to advertise encrypted BLE4.2 advertisements nor scan for them.
bool_flag(
    name = "enable_rust_ldt",
    build_setting_default = False,
)

config_setting(
    name = "no_link_with_rust",
    flag_values = {
        ":enable_rust_ldt": "false",
    },
)

# Current google3 does not support Rust on Android or Windows.
selects.config_setting_group(
    name = "norust_or_windows_or_android",
    match_any = [
        ":no_link_with_rust",
        "@platforms//os:windows",
        "@platforms//os:android",
    ],
)

cc_library(
    name = "ldt_stub",
    srcs = ["np_ldt.c"],
    hdrs = ["np_ldt.h"],
)

cc_library(
    name = "internal",
    srcs = [
        "action_factory.cc",
        "advertisement_decoder.cc",
        "advertisement_factory.cc",
        "base_broadcast_request.cc",
        "broadcast_manager.cc",
        "connection_authenticator.cc",
        "credential_manager_impl.cc",
        "ldt.cc",
        "scan_manager.cc",
        "service_controller_impl.cc",
    ],
    hdrs = [
        "action_factory.h",
        "advertisement_decoder.h",
        "advertisement_factory.h",
        "base_broadcast_request.h",
        "broadcast_manager.h",
        "connection_authenticator.h",
        "credential_manager.h",
        "credential_manager_impl.h",
        "ldt.h",
        "scan_manager.h",
        "service_controller.h",
        "service_controller_impl.h",
    ],
    defines = select({
        ":norust_or_windows_or_android": [],
        "//conditions:default": ["USE_RUST_LDT=1"],
    }),
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        "//internal/crypto",
        "//internal/crypto_cros",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/proto:credential_cc_proto",
        "//internal/proto:local_credential_cc_proto",
        "//internal/proto:metadata_cc_proto",
        "//presence:types",
        "//presence/implementation/mediums",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/random:distributions",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
    ] + select({
        ":norust_or_windows_or_android": [":ldt_stub"],
        "//conditions:default": ["//third_party/beto_core/nearby/presence/ldt_np_adv_ffi"],
    }),
)

cc_library(
    name = "sensor_fusion",
    hdrs = ["sensor_fusion.h"],
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        "//presence:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "internal_test",
    testonly = True,
    srcs = [
    ],
    hdrs = [
        "mock_service_controller.h",
    ],
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        ":internal",
        "//presence",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "advertisement_decoder_test",
    size = "small",
    srcs = ["advertisement_decoder_test.cc"],
    deps = [
        ":internal",
        "//internal/platform:base",
        "//internal/proto:credential_cc_proto",
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "advertisement_factory_test",
    size = "small",
    srcs = ["advertisement_factory_test.cc"],
    deps = [
        ":internal",
        "//internal/platform:base",
        "//internal/proto:credential_cc_proto",
        "//presence:types",
        "//presence/implementation/mediums",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "broadcast_manager_test",
    size = "small",
    srcs = ["broadcast_manager_test.cc"],
    deps = [
        ":internal",
        "//internal/platform:base",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/proto:credential_cc_proto",
        "//presence/implementation/mediums",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "ldt_test",
    size = "small",
    srcs = ["ldt_test.cc"],
    deps = [
        ":internal",
        "//internal/platform:base",
        "//internal/proto:credential_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "base_broadcast_request_test",
    srcs = ["base_broadcast_request_test.cc"],
    deps = [
        ":internal",
        "//internal/proto:credential_cc_proto",
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/types:variant",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "action_factory_test",
    size = "small",
    srcs = ["action_factory_test.cc"],
    deps = [
        ":internal",
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "connection_authenticator_test",
    size = "small",
    srcs = ["connection_authenticator_test.cc"],
    deps = [
        ":internal",
        "//internal/crypto",
        "//internal/crypto_cros",
        "//internal/proto:credential_cc_proto",
        "//internal/proto:local_credential_cc_proto",
        "//presence/proto:presence_frame_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "credential_manager_impl_test",
    size = "small",
    srcs = ["credential_manager_impl_test.cc"],
    deps = [
        ":internal",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/proto:credential_cc_proto",
        "//net/proto2/contrib/parse_proto:testing",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)

cc_test(
    name = "scan_manager_test",
    size = "small",
    srcs = ["scan_manager_test.cc"],
    deps = [
        ":internal",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//presence/implementation/mediums",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)
