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

package_group(
    name = "nearby_fastpair",
    packages = [
        "//fastpair/...",
    ],
)

cc_library(
    name = "fake_initial_pair_plugin",
    hdrs = ["fake_initial_pair_plugin.h"],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:private"],
    deps = [
        "//fastpair:fast_pair_events",
        "//fastpair:fast_pair_plugin",
        "//fastpair:fast_pair_seeker",
        "//fastpair/common",
        "//internal/platform:types",
    ],
)

cc_library(
    name = "windows_admin_plugin",
    srcs = ["windows_admin_plugin.cc"],
    hdrs = ["windows_admin_plugin.h"],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//fastpair:fast_pair_events",
        "//fastpair:fast_pair_plugin",
        "//fastpair:fast_pair_seeker",
        "//fastpair:fast_pair_service",
        "//fastpair/common",
        "//fastpair/ui:fast_pair_ui",
        "//internal/platform:types",
    ],
)

cc_test(
    name = "fake_initial_pair_plugin_test",
    size = "small",
    srcs = [
        "fake_initial_pair_plugin_test.cc",
    ],
    deps = [
        ":fake_initial_pair_plugin",
        "//fastpair:test_support",
        "//fastpair/common",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_fast_pair_plugin",
    testonly = True,
    srcs = ["fake_fast_pair_plugin.cc"],
    hdrs = [
        "fake_fast_pair_plugin.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair:fast_pair_events",
        "//fastpair:fast_pair_plugin",
        "//fastpair:fast_pair_seeker",
        "//fastpair/common",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)
