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
    name = "message_stream",
    srcs = [
        "medium.cc",
        "message_stream.cc",
    ],
    hdrs = [
        "medium.h",
        "message.h",
        "message_stream.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "fake_provider",
    testonly = True,
    srcs = ["fake_provider.cc"],
    hdrs = [
        "fake_provider.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":fake_gatt_callbacks",
        ":message_stream",
        "//fastpair/common",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "@boringssl//:crypto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_library(
    name = "fake_gatt_callbacks",
    testonly = True,
    hdrs = [
        "fake_gatt_callbacks.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//internal/platform:comm",
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "fake_medium_observer",
    testonly = True,
    hdrs = [
        "fake_medium_observer.h",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        ":message_stream",
        "//fastpair/common",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_for_library_testonly",
    ],
)

cc_test(
    name = "medium_test",
    size = "small",
    srcs = [
        "medium_test.cc",
    ],
    deps = [
        ":fake_medium_observer",
        ":fake_provider",
        ":message_stream",
        "//fastpair/common",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//testing/fuzzing:fuzztest",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "message_stream_test",
    size = "small",
    srcs = [
        "message_stream_test.cc",
    ],
    deps = [
        ":fake_medium_observer",
        ":fake_provider",
        ":message_stream",
        "//fastpair/common",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//testing/fuzzing:fuzztest",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
