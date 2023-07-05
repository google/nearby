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

licenses(["notice"])

cc_library(
    name = "server_access",
    srcs = [
        "fast_pair_client_impl.cc",
        "fast_pair_http_notifier.cc",
        "fast_pair_metadata_downloader.cc",
        "fast_pair_metadata_downloader_impl.cc",
        "fast_pair_repository.cc",
        "fast_pair_repository_impl.cc",
    ],
    hdrs = [
        "fast_pair_client.h",
        "fast_pair_client_impl.h",
        "fast_pair_http_notifier.h",
        "fast_pair_metadata_downloader.h",
        "fast_pair_metadata_downloader_impl.h",
        "fast_pair_repository.h",
        "fast_pair_repository_impl.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/proto:proto_to_json",
        "//fastpair/repository",
        "//internal/account",
        "//internal/auth:types",
        "//internal/base",
        "//internal/base:bluetooth_address",
        "//internal/crypto",
        "//internal/network:nearby_http_client",
        "//internal/network:types",
        "//internal/platform:logging",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "test_support",
    srcs = ["fake_fast_pair_repository.cc"],
    hdrs = [
        "fake_fast_pair_client.h",
        "fake_fast_pair_repository.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = [
        "mock_fast_pair_repository.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":server_access",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "fast_pair_metadata_downloader_impl_test",
    srcs = [
        "fast_pair_metadata_downloader_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository:fake_fast_pair_metadata_repository",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_repository_impl_test",
    srcs = [
        "fast_pair_repository_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        ":test_support",
        "//fastpair/common",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_http_notifier_test",
    srcs = [
        "fast_pair_http_notifier_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_repository_test",
    srcs = [
        "fast_pair_repository_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_client_impl_test",
    srcs = [
        "fast_pair_client_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/proto:proto_builder",
        "//internal/account",
        "//internal/account:test_support",
        "//internal/auth:credential",
        "//internal/network:types",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/test",
        "//internal/test/google3_only:test",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
