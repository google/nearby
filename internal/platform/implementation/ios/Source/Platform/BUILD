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
licenses(["notice"])

package(default_visibility = ["//internal/platform/implementation/ios:__subpackages__"])

objc_library(
    name = "Platform",
    srcs = [
        "crypto.mm",
        "input_file.mm",
        "log_message.mm",
        "multi_thread_executor.mm",
        "scheduled_executor.mm",
        "utils.mm",
        "wifi_lan.mm",
    ],
    hdrs = [
        "input_file.h",
        "log_message.h",
        "multi_thread_executor.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
        "utils.h",
        "wifi_lan.h",
    ],
    sdk_frameworks = [
        "CoreBluetooth",
        "CoreFoundation",
    ],
    deps = [
        ":Platform_cc",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/ios/Source/Mediums",
        "//internal/platform/implementation/ios/Source/Shared",
        "//internal/platform/implementation/shared:file",
        "//third_party/objective_c/google_toolbox_for_mac:GTM_Logger",
    ],
)

cc_library(
    name = "Platform_cc",
    srcs = [
        "condition_variable.cc",
        "count_down_latch.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_uint32.h",
        "condition_variable.h",
        "count_down_latch.h",
        "mutex.h",
    ],
    deps = [
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "Platform_cc_test",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_uint32_test.cc",
        "condition_variable_test.cc",
        "count_down_latch_test.cc",
        "mutex_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":Platform_cc",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_nisaba//nisaba/port:thread_pool/fiber",
    ],
)
