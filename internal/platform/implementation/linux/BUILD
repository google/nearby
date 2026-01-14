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
    name = "types",
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "atomic_uint32.h",
        "condition_variable.h",
        "device_info.h",
        "executor.h",
        "future.h",
        "mutex.h",
        "preferences_manager.h",
        "preferences_repository.h",
        "scheduled_executor.h",
        "submittable_executor.h",
        "timer.h",
        "thread_pool.h",
        #"log_message.h",
        "utils.h",
    ],
    srcs = [
        "device_info.cc",
        #"log_message.cc",
        "timer.cc",
    ],
    copts = ["-lrt"],
    visibility = ["//third_party/nearby/sharing/internal/impl/linux:__pkg__"],
    deps = [
        ":comm",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings",
        "@sdbus_cpp//:libsystemd",
        "@sdbus_cpp//:sdbus_cpp",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "dbus.h",
        "stream.h",
    ],
    deps = [
        ":crypto",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/linux/generated:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@sdbus_cpp//:libsystemd",
        "@sdbus_cpp//:sdbus_cpp",
    ],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:types",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
])

cc_library(
    name = "linux",
    srcs = [
        "dbus.cc",
        "executor.cc",
        "platform.cc",
        "preferences_manager.cc",
        "preferences_repository.cc",
        "scheduled_executor.cc",
        "stream.cc",
        "submittable_executor.cc",
        "system_clock.cc",
        "thread_pool.cc",
        "utils.cc",
    ],
    linkopts = ["-lcurl"],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//location/nearby:__subpackages__",
        "//presence:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "//internal/platform/implementation/linux/generated:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@nlohmann_json//:json",
        "@sdbus_cpp//:libsystemd",
        "@sdbus_cpp//:sdbus_cpp",
    ],
)

cc_library(
    name = "test_utils",
    srcs = [
        "test_utils.cc",
    ],
    hdrs = [
        "test_data.h",
        "test_utils.h",
    ],
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
    deps = [
        "//internal/platform:base",
        "@nlohmann_json//:json",
        ":types",
    ],
)

cc_test(
    name = "impl_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "mutex_test.cc",
        "utils_test.cc",
        # "bluetooth_adapter_test.cc",
        # "crypto_test.cc",
        # "device_info_test.cc",
        # "executor_test.cc",
        # "file_path_test.cc",
        # "http_loader_test.cc",
        # "preferences_manager_test.cc",
        # "preferences_repository_test.cc",
        # "scheduled_executor_test.cc",
        # "submittable_executor_test.cc",
        # "thread_pool_test.cc",
        # "timer_test.cc",
    ],
    tags = ["notap"],
    deps = [
        ":comm",
        ":crypto",
        ":test_utils",
        ":types",
        ":linux",
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
