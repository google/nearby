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

load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

# Core platform tests
cc_test(
    name = "atomic_boolean_test",
    size = "small",
    srcs = ["atomic_boolean_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "atomic_reference_test",
    size = "small",
    srcs = ["atomic_reference_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "count_down_latch_test",
    size = "small",
    srcs = ["count_down_latch_test.cc"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "crypto_test",
    size = "small",
    srcs = ["crypto_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:crypto",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "device_info_test",
    size = "small",
    srcs = ["device_info_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "device_paths_test",
    size = "small",
    srcs = ["device_paths_test.cc"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "executor_test",
    size = "small",
    srcs = ["executor_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "file_test",
    size = "small",
    srcs = ["file_test.cc"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "http_loader_test",
    size = "small",
    srcs = ["http_loader_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:linux",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "platform_test",
    size = "small",
    srcs = ["platform_test.cc"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "preferences_manager_test",
    size = "small",
    srcs = ["preferences_manager_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "preferences_repository_test",
    size = "small",
    srcs = ["preferences_repository_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "scheduled_executor_test",
    size = "small",
    srcs = ["scheduled_executor_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "string_utils_test",
    size = "small",
    srcs = ["string_utils_test.cc"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "submittable_executor_test",
    size = "small",
    srcs = ["submittable_executor_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "task_scheduler_test",
    size = "small",
    srcs = ["task_scheduler_test.cc"],
    deps = [
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "thread_pool_test",
    size = "small",
    srcs = ["thread_pool_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "timer_test",
    size = "small",
    srcs = ["timer_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "utils_test",
    size = "small",
    srcs = ["utils_test.cc"],
    deps = [
        "//internal/platform/implementation/linux:types",
        "@com_github_google_googletest//:gtest_main",
    ],
)


