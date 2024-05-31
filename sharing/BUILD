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
    name = "connection_types",
    hdrs = ["nearby_connections_types.h"],
    deps = [
        "//internal/base:files",
        "//internal/crypto_cros",
        "//internal/interop:authentication_status",
        "//sharing/common:compatible_u8_string",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "attachments",
    srcs = [
        "attachment.cc",
        "attachment_container.cc",
        "file_attachment.cc",
        "text_attachment.cc",
        "wifi_credentials_attachment.cc",
    ],
    hdrs = [
        "attachment.h",
        "attachment_container.h",
        "file_attachment.h",
        "text_attachment.h",
        "wifi_credentials_attachment.h",
    ],
    visibility = [
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/sharing/sdk/quick_share_server:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "//internal/network:url",
        "//sharing/common:compatible_u8_string",
        "//sharing/common:enum",
        "//sharing/internal/base",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "advertisement.cc",
        "attachment_info.cc",
        "share_target.cc",
    ],
    hdrs = [
        "advertisement.h",
        "attachment_info.h",
        "nearby_connection.h",
        "nearby_connections_manager.h",
        "nearby_sharing_decoder.h",
        "share_target.h",
    ],
    visibility = [
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/sharing/sdk/quick_share_server:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":attachments",
        ":connection_types",
        "//internal/network:url",
        "//sharing/common:enum",
        "//sharing/internal/public:logging",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "transfer_metadata",
    srcs = [
        "transfer_metadata.cc",
        "transfer_metadata_builder.cc",
    ],
    hdrs = [
        "transfer_metadata.h",
        "transfer_metadata_builder.h",
    ],
    visibility = [
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/sharing/sdk/quick_share_server:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "nearby_sharing_service",
    srcs = [
        "incoming_frames_reader.cc",
        "incoming_share_target_info.cc",
        "nearby_connection_impl.cc",
        "nearby_connections_manager.cc",
        "nearby_connections_manager_factory.cc",
        "nearby_connections_manager_impl.cc",
        "nearby_connections_service.cc",
        "nearby_connections_service_impl.cc",
        "nearby_connections_stream_buffer_manager.cc",
        "nearby_file_handler.cc",
        "nearby_share_profile_info_provider_impl.cc",
        "nearby_sharing_decoder_impl.cc",
        "nearby_sharing_service.cc",
        "nearby_sharing_service_extension.cc",
        "nearby_sharing_service_factory.cc",
        "nearby_sharing_service_impl.cc",
        "nearby_sharing_settings.cc",
        "nearby_sharing_util.cc",
        "outgoing_share_target_info.cc",
        "paired_key_verification_runner.cc",
        "payload_tracker.cc",
        "share_target_info.cc",
        "transfer_manager.cc",
        "wrapped_share_target_discovered_callback.cc",
    ],
    hdrs = [
        "connection_lifecycle_listener.h",
        "constants.h",
        "endpoint_discovery_listener.h",
        "incoming_frames_reader.h",
        "incoming_share_target_info.h",
        "nearby_connection_impl.h",
        "nearby_connections_manager_factory.h",
        "nearby_connections_manager_impl.h",
        "nearby_connections_service.h",
        "nearby_connections_service_impl.h",
        "nearby_connections_stream_buffer_manager.h",
        "nearby_file_handler.h",
        "nearby_share_profile_info_provider_impl.h",
        "nearby_sharing_decoder_impl.h",
        "nearby_sharing_service.h",
        "nearby_sharing_service_extension.h",
        "nearby_sharing_service_factory.h",
        "nearby_sharing_service_impl.h",
        "nearby_sharing_settings.h",
        "nearby_sharing_util.h",
        "outgoing_share_target_info.h",
        "paired_key_verification_runner.h",
        "payload_listener.h",
        "payload_tracker.h",
        "share_target_discovered_callback.h",
        "share_target_info.h",
        "transfer_manager.h",
        "transfer_update_callback.h",
        "wrapped_share_target_discovered_callback.h",
    ],
    copts = [
        "-DNEARBY_SHARING_DLL",
    ],
    visibility = [
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/sharing/sdk/quick_share_server:__pkg__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":attachments",
        ":connection_types",
        ":transfer_metadata",
        ":types",
        "//connections:core",
        "//connections:core_types",
        "//connections/implementation:internal",
        "//internal/analytics:event_logger",
        "//internal/base",
        "//internal/base:bluetooth_address",
        "//internal/base:files",
        "//internal/flags:nearby_flags",
        "//internal/network:url",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation:account_manager",
        "//internal/platform/implementation:types",
        "//proto:sharing_enums_cc_proto",
        "//sharing/analytics",
        "//sharing/certificates",
        "//sharing/common",
        "//sharing/common:compatible_u8_string",
        "//sharing/common:enum",
        "//sharing/contacts",
        "//sharing/fast_initiation:nearby_fast_initiation",
        "//sharing/flags/generated:generated_flags",
        "//sharing/internal/api:platform",
        "//sharing/internal/base",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:nearby_context",
        "//sharing/internal/public:types",
        "//sharing/local_device_data",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "//sharing/scheduling",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "test_support",
    testonly = True,
    srcs = [
        "fake_nearby_connection.cc",
        "fake_nearby_connections_manager.cc",
        "fake_nearby_sharing_service.cc",
    ],
    hdrs = [
        "fake_nearby_connection.h",
        "fake_nearby_connections_manager.h",
        "fake_nearby_sharing_service.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":attachments",
        ":connection_types",
        ":nearby_sharing_service",
        ":transfer_metadata",
        ":types",
        "//internal/base",
        "//sharing/common:enum",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:logging",
        "//sharing/local_device_data",
        "//sharing/proto:enums_cc_proto",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "advertisement_test",
    srcs = ["advertisement_test.cc"],
    deps = [
        ":types",
        "//sharing/common:enum",
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
    name = "nearby_connections_types_payload_test",
    srcs = ["nearby_connections_types_payload_test.cc"],
    deps = [
        ":connection_types",
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
    name = "paired_key_verification_runner_test",
    size = "small",
    srcs = ["paired_key_verification_runner_test.cc"],
    deps = [
        ":nearby_sharing_service",
        ":test_support",
        ":types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//proto:sharing_enums_cc_proto",
        "//sharing/certificates",
        "//sharing/certificates:test_support",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:types",
        "//sharing/internal/test:nearby_test",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_sharing_service_test",
    srcs = [
        "nearby_sharing_service_test.cc",
    ],
    deps = [
        ":nearby_sharing_service",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "incoming_frames_reader_test",
    srcs = ["incoming_frames_reader_test.cc"],
    deps = [
        ":nearby_sharing_service",
        ":test_support",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/public:types",
        "//sharing/internal/test:nearby_test",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_connection_impl_test",
    srcs = ["nearby_connection_impl_test.cc"],
    deps = [
        ":nearby_sharing_service",
        ":test_support",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/test:nearby_test",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_connections_manager_impl_test",
    srcs = [
        "fake_nearby_connections_service.h",
        "nearby_connections_manager_impl_test.cc",
    ],
    deps = [
        ":connection_types",
        ":nearby_sharing_service",
        ":types",
        "//internal/flags:nearby_flags",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/common:enum",
        "//sharing/flags/generated:generated_flags",
        "//sharing/internal/public:types",
        "//sharing/internal/test:nearby_test",
        "//sharing/proto:enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_sharing_service_impl_test",
    srcs = ["nearby_sharing_service_impl_test.cc"],
    shard_count = 10,
    deps = [
        ":attachments",
        ":connection_types",
        ":nearby_sharing_service",
        ":test_support",
        ":transfer_metadata",
        ":types",
        "//base:casts",
        "//internal/account",
        "//internal/flags:nearby_flags",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/certificates",
        "//sharing/certificates:test_support",
        "//sharing/common",
        "//sharing/common:compatible_u8_string",
        "//sharing/common:enum",
        "//sharing/contacts",
        "//sharing/contacts:test_support",
        "//sharing/fast_initiation:nearby_fast_initiation",
        "//sharing/fast_initiation:test_support",
        "//sharing/flags/generated:generated_flags",
        "//sharing/internal/api:mock_sharing_platform",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:types",
        "//sharing/internal/test:nearby_test",
        "//sharing/local_device_data",
        "//sharing/local_device_data:test_support",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_test(
    name = "nearby_connections_stream_buffer_manager_test",
    srcs = ["nearby_connections_stream_buffer_manager_test.cc"],
    deps = [
        ":nearby_sharing_service",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_connections_types_test",
    srcs = ["nearby_connections_types_test.cc"],
    deps = [
        ":connection_types",
        ":nearby_sharing_service",
        ":types",
        "//connections:core_types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_file_handler_test",
    srcs = ["nearby_file_handler_test.cc"],
    deps = [
        ":nearby_sharing_service",
        "//internal/base:files",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_share_profile_info_provider_impl_test",
    srcs = ["nearby_share_profile_info_provider_impl_test.cc"],
    deps = [
        ":nearby_sharing_service",
        "//internal/platform/implementation:account_manager",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_sharing_service_extension_test",
    srcs = ["nearby_sharing_service_extension_test.cc"],
    deps = [
        ":attachments",
        ":nearby_sharing_service",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/test:nearby_test",
        "//sharing/local_device_data:test_support",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_sharing_settings_test",
    srcs = ["nearby_sharing_settings_test.cc"],
    shard_count = 5,
    deps = [
        ":nearby_sharing_service",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/common",
        "//sharing/common:compatible_u8_string",
        "//sharing/common:enum",
        "//sharing/internal/test:nearby_test",
        "//sharing/local_device_data:test_support",
        "//sharing/proto:enums_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "payload_tracker_test",
    srcs = ["payload_tracker_test.cc"],
    deps = [
        ":attachments",
        ":connection_types",
        ":nearby_sharing_service",
        ":transfer_metadata",
        ":types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/test:nearby_test",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "share_target_test",
    srcs = ["share_target_test.cc"],
    deps = [
        ":attachments",
        ":types",
        "//internal/network:url",
        "//sharing/common:enum",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "text_attachment_test",
    srcs = ["text_attachment_test.cc"],
    deps = [
        ":attachments",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "transfer_manager_test",
    srcs = ["transfer_manager_test.cc"],
    deps = [
        ":connection_types",
        ":nearby_sharing_service",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/test:nearby_test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "transfer_metadata_test",
    srcs = ["transfer_metadata_test.cc"],
    deps = [
        ":transfer_metadata",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "share_target_info_test",
    srcs = ["share_target_info_test.cc"],
    deps = [
        ":nearby_sharing_service",
        ":transfer_metadata",
        ":types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "attachment_container_test",
    srcs = ["attachment_container_test.cc"],
    deps = [
        ":attachments",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "wrapped_share_target_discovered_callback_test",
    srcs = ["wrapped_share_target_discovered_callback_test.cc"],
    deps = [
        ":nearby_sharing_service",
        ":types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
