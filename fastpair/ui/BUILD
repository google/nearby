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
    name = "fast_pair_ui",
    srcs = [
        "fast_pair/fast_pair_notification_controller.cc",
        "fast_pair/fast_pair_presenter_impl.cc",
        "ui_broker_impl.cc",
    ],
    hdrs = [
        "actions.h",
        "fast_pair/fast_pair_notification_controller.h",
        "fast_pair/fast_pair_presenter.h",
        "fast_pair/fast_pair_presenter_impl.h",
        "ui_broker.h",
        "ui_broker_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/repository",
        "//internal/base",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "fake_fast_pair_ui",
    testonly = True,
    hdrs = [
        "fast_pair/fake_fast_pair_notification_controller_observer.h",
        "fast_pair/fake_fast_pair_presenter.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":fast_pair_ui",
        "//fastpair/common",
        "//fastpair/repository",
        "//internal/platform:types",
    ],
)

cc_library(
    name = "mock_fast_pair_ui",
    hdrs = [
        "fast_pair/mock_fast_pair_notification_controller.h",
        "mock_ui_broker.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":fast_pair_ui",
        "//fastpair/common",
        "//fastpair/repository",
        "//internal/base",
        "//testing/base/public:gunit_for_library",
    ],
)

cc_test(
    name = "fast_pair_notification_controller_test",
    srcs = [
        "fast_pair/fast_pair_notification_controller_test.cc",
    ],
    deps = [
        ":fake_fast_pair_ui",
        ":fast_pair_ui",
        "//fastpair/common",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_presenter_test",
    srcs = [
        "fast_pair/fast_pair_presenter_impl_test.cc",
    ],
    deps = [
        ":fake_fast_pair_ui",
        ":fast_pair_ui",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ui_broker_test",
    srcs = [
        "ui_broker_impl_test.cc",
    ],
    deps = [
        ":fake_fast_pair_ui",
        ":fast_pair_ui",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
