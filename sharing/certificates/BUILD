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
    name = "certificates",
    srcs = [
        "common.cc",
        "nearby_share_certificate_manager.cc",
        "nearby_share_certificate_manager_impl.cc",
        "nearby_share_certificate_storage.cc",
        "nearby_share_certificate_storage_impl.cc",
        "nearby_share_decrypted_public_certificate.cc",
        "nearby_share_encrypted_metadata_key.cc",
        "nearby_share_private_certificate.cc",
    ],
    hdrs = [
        "common.h",
        "constants.h",
        "nearby_share_certificate_manager.h",
        "nearby_share_certificate_manager_impl.h",
        "nearby_share_certificate_storage.h",
        "nearby_share_certificate_storage_impl.h",
        "nearby_share_decrypted_public_certificate.h",
        "nearby_share_encrypted_metadata_key.h",
        "nearby_share_private_certificate.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/base",
        "//internal/crypto_cros",
        "//internal/platform:types",
        "//internal/platform/implementation:account_manager",
        "//internal/platform/implementation:types",
        "//sharing/common",
        "//sharing/common:enum",
        "//sharing/contacts",
        "//sharing/internal/api:platform",
        "//sharing/internal/base",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:types",
        "//sharing/local_device_data",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "//sharing/scheduling",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_library(
    name = "test_support",
    testonly = True,
    srcs = [
        "fake_nearby_share_certificate_manager.cc",
        "fake_nearby_share_certificate_storage.cc",
        "test_util.cc",
    ],
    hdrs = [
        "fake_nearby_share_certificate_manager.h",
        "fake_nearby_share_certificate_storage.h",
        "test_util.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":certificates",
        "//internal/base:bluetooth_address",
        "//internal/crypto_cros",
        "//sharing/common:enum",
        "//sharing/contacts",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:types",
        "//sharing/local_device_data",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "certificates_test",
    srcs = [
        "common_test.cc",
        "nearby_share_certificate_manager_impl_test.cc",
        "nearby_share_certificate_storage_impl_test.cc",
        "nearby_share_decrypted_public_certificate_test.cc",
        "nearby_share_private_certificate_test.cc",
    ],
    deps = [
        ":certificates",
        ":test_support",
        "//internal/platform/implementation:account_manager",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/common",
        "//sharing/common:enum",
        "//sharing/contacts:test_support",
        "//sharing/internal/api:mock_sharing_platform",
        "//sharing/internal/public:logging",
        "//sharing/internal/test:nearby_test",
        "//sharing/local_device_data:test_support",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:share_cc_proto",
        "//sharing/scheduling",
        "//sharing/scheduling:test_support",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
