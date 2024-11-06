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
        "//internal/crypto_cros",  # buildcleaner: keep
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
        "//location/nearby/testing/nearby_native:__subpackages__",
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
        "share_target.cc",
    ],
    hdrs = [
        "advertisement.h",
        "constants.h",
        "nearby_connection.h",
        "nearby_connections_manager.h",
        "share_target.h",
    ],
    visibility = [
        "//location/nearby/cpp/sharing:__subpackages__",
        "//location/nearby/sharing/sdk/quick_share_server:__pkg__",
        "//location/nearby/testing/nearby_native:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":connection_types",
        "//internal/network:url",
        "//sharing/common:enum",
        "//sharing/internal/public:logging",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
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
        "//location/nearby/testing/nearby_native:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "nearby_sharing_decoder",
    hdrs = ["nearby_sharing_decoder.h"],
    visibility = [
        "//sharing/fuzzing:__pkg__",
    ],
    deps = [
        ":types",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "incoming_frame_reader",
    srcs = ["incoming_frames_reader.cc"],
    hdrs = ["incoming_frames_reader.h"],
    deps = [
        ":thread_timer",
        ":types",
        "//internal/platform:types",
        "//sharing/internal/public:logging",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "paired_key_verification_runner",
    srcs = ["paired_key_verification_runner.cc"],
    hdrs = ["paired_key_verification_runner.h"],
    deps = [
        ":incoming_frame_reader",
        ":types",
        "//internal/platform:types",
        "//proto:sharing_enums_cc_proto",
        "//sharing/certificates",
        "//sharing/internal/public:logging",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "thread_timer",
    srcs = ["thread_timer.cc"],
    hdrs = ["thread_timer.h"],
    deps = [
        "//internal/platform:types",
        "//sharing/internal/public:logging",
        "@com_google_absl//absl/debugging:leak_check",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "share_session",
    srcs = [
        "incoming_share_session.cc",
        "nearby_file_handler.cc",
        "outgoing_share_session.cc",
        "payload_tracker.cc",
        "share_session.cc",
    ],
    hdrs = [
        "incoming_share_session.h",
        "nearby_file_handler.h",
        "outgoing_share_session.h",
        "payload_tracker.h",
        "share_session.h",
    ],
    deps = [
        ":attachments",
        ":connection_types",
        ":incoming_frame_reader",
        ":paired_key_verification_runner",
        ":thread_timer",
        ":transfer_metadata",
        ":types",
        "//internal/base:files",
        "//internal/platform:types",
        "//proto:sharing_enums_cc_proto",
        "//sharing/analytics",
        "//sharing/certificates",
        "//sharing/common:compatible_u8_string",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:logging",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "nearby_connection_impl",
    srcs = ["nearby_connection_impl.cc"],
    hdrs = ["nearby_connection_impl.h"],
    deps = [
        ":connection_types",
        ":types",
        "//internal/platform:types",
        "//sharing/internal/public:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "nearby_sharing_service",
    srcs = [
        "nearby_connections_manager.cc",
        "nearby_connections_manager_factory.cc",
        "nearby_connections_manager_impl.cc",
        "nearby_connections_service.cc",
        "nearby_connections_service_impl.cc",
        "nearby_connections_stream_buffer_manager.cc",
        "nearby_sharing_service.cc",
        "nearby_sharing_service_factory.cc",
        "nearby_sharing_service_impl.cc",
        "nearby_sharing_settings.cc",
        "nearby_sharing_util.cc",
        "transfer_manager.cc",
        "wrapped_share_target_discovered_callback.cc",
    ],
    hdrs = [
        "connection_lifecycle_listener.h",
        "endpoint_discovery_listener.h",
        "nearby_connections_manager_factory.h",
        "nearby_connections_manager_impl.h",
        "nearby_connections_service.h",
        "nearby_connections_service_impl.h",
        "nearby_connections_stream_buffer_manager.h",
        "nearby_sharing_service.h",
        "nearby_sharing_service_extension.h",
        "nearby_sharing_service_factory.h",
        "nearby_sharing_service_impl.h",
        "nearby_sharing_settings.h",
        "nearby_sharing_util.h",
        "payload_listener.h",
        "share_target_discovered_callback.h",
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
        "//location/nearby/testing/nearby_native:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        ":attachments",
        ":connection_types",
        ":incoming_frame_reader",
        ":nearby_connection_impl",
        ":nearby_sharing_decoder",
        ":paired_key_verification_runner",
        ":share_session",
        ":thread_timer",
        ":transfer_metadata",
        ":types",
        "//connections:core",
        "//connections:core_types",
        "//connections/implementation:internal",
        "//internal/analytics:event_logger",
        "//internal/base",
        "//internal/base:bluetooth_address",
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
        "//internal/platform:types",
        "//sharing/common:enum",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:logging",
        "//sharing/local_device_data",
        "//sharing/proto:enums_cc_proto",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "attachment_compare",
    testonly = True,
    srcs = ["attachment_compare.cc"],
    hdrs = ["attachment_compare.h"],
    deps = [
        ":attachments",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "transfer_metadata_matchers",
    testonly = True,
    hdrs = ["transfer_metadata_matchers.h"],
    deps = [
        ":transfer_metadata",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "advertisement_test",
    srcs = ["advertisement_test.cc"],
    deps = [
        ":types",
        "//sharing/common:enum",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_connections_types_payload_test",
    srcs = ["nearby_connections_types_payload_test.cc"],
    deps = [
        ":connection_types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "paired_key_verification_runner_test",
    size = "small",
    srcs = ["paired_key_verification_runner_test.cc"],
    deps = [
        ":incoming_frame_reader",
        ":paired_key_verification_runner",
        ":test_support",
        ":types",
        "//internal/platform:types",
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
        ":incoming_frame_reader",
        ":nearby_connection_impl",
        ":test_support",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/internal/public:logging",
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
        ":incoming_frame_reader",
        ":nearby_connection_impl",
        ":test_support",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
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
        "@com_google_absl//absl/container:flat_hash_set",
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
        ":share_session",
        ":test_support",
        ":transfer_metadata",
        ":transfer_metadata_matchers",
        ":types",
        "//base:casts",
        "//internal/analytics:mock_event_logger",
        "//internal/auth:auth_status_util",
        "//internal/flags:nearby_flags",
        "//internal/platform/implementation:signin_attempt",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//internal/test:mocks",
        "//sharing/analytics",
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
        ":share_session",
        "//internal/base:files",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//sharing/internal/api:mock_sharing_platform",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
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
        ":share_session",
        ":transfer_metadata",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
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
    name = "share_session_test",
    srcs = ["share_session_test.cc"],
    deps = [
        ":paired_key_verification_runner",
        ":share_session",
        ":test_support",
        ":transfer_metadata",
        ":transfer_metadata_matchers",
        ":types",
        "//internal/analytics:mock_event_logger",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/analytics",
        "//sharing/certificates:test_support",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "attachment_container_test",
    srcs = ["attachment_container_test.cc"],
    deps = [
        ":attachment_compare",
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

cc_test(
    name = "outgoing_share_session_test",
    srcs = ["outgoing_share_session_test.cc"],
    deps = [
        ":attachments",
        ":connection_types",
        ":paired_key_verification_runner",
        ":share_session",
        ":test_support",
        ":transfer_metadata",
        ":transfer_metadata_matchers",
        ":types",
        "//internal/analytics:mock_event_logger",
        "//internal/network:url",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/analytics",
        "//sharing/certificates:test_support",
        "//sharing/common:enum",
        "//sharing/proto:wire_format_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "incoming_share_session_test",
    srcs = ["incoming_share_session_test.cc"],
    deps = [
        ":attachment_compare",
        ":attachments",
        ":connection_types",
        ":paired_key_verification_runner",
        ":share_session",
        ":test_support",
        ":transfer_metadata",
        ":transfer_metadata_matchers",
        ":types",
        "//internal/analytics:mock_event_logger",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//proto:sharing_enums_cc_proto",
        "//sharing/analytics",
        "//sharing/internal/public:logging",
        "//sharing/proto:wire_format_cc_proto",
        "//sharing/proto/analytics:sharing_log_cc_proto",
        "//third_party/protobuf",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "thread_timer_test",
    srcs = ["thread_timer_test.cc"],
    deps = [
        ":thread_timer",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nearby_connections_service_test",
    srcs = ["nearby_connections_service_test.cc"],
    deps = [
        ":connection_types",
        ":nearby_sharing_service",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
