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

cc_test(
    name = "linux_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "device_info_test.cc",
        "device_paths_test.cc",
        "executor_test.cc",
        "file_test.cc",
        "http_loader_test.cc",
        "platform_test.cc",
        "preferences_manager_test.cc",
        "preferences_repository_test.cc",
        "scheduled_executor_test.cc",
        "string_utils_test.cc",
        "submittable_executor_test.cc",
        "task_scheduler_test.cc",
        "thread_pool_test.cc",
        "timer_test.cc",
        "utils_test.cc",
    ],
    deps = [
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:logging",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation/linux:comm",
        "//internal/platform/implementation/linux:crypto",
        "//internal/platform/implementation/linux:linux",
        "//internal/platform/implementation/linux:types",
        "//internal/platform/implementation/linux:test_utils",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:json",
        "@sdbus_cpp//:sdbus_cpp",
    ],
)


