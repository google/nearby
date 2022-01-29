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
load("//tools/build_defs/cc:cc_fake_binary.bzl", "cc_fake_binary")

licenses(["notice"])

cc_library(
    name = "types",
    srcs = [
        "monitored_runnable.cc",
        "pending_job_registry.cc",
        "pipe.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "cancelable.h",
        "cancelable_alarm.h",
        "cancellable_task.h",
        "condition_variable.h",
        "core_config.h",
        "count_down_latch.h",
        "crypto.h",
        "file.h",
        "future.h",
        "lockable.h",
        "logging.h",
        "monitored_runnable.h",
        "multi_thread_executor.h",
        "mutex.h",
        "mutex_lock.h",
        "pending_job_registry.h",
        "pipe.h",
        "scheduled_executor.h",
        "settable_future.h",
        "single_thread_executor.h",
        "submittable_executor.h",
        "system_clock.h",
        "thread_check_callable.h",
        "thread_check_runnable.h",
    ],
    visibility = [
        "//cpp/core:__subpackages__",
        "//cpp/platform/base:__pkg__",
        "//cpp/platform/impl/ios:__subpackages__",
        "//cpp/platform/impl/windows:__subpackages__",
        "//cpp/platform/public:__pkg__",
        "//internal/analytics:__subpackages__",
    ],
    deps = [
        ":logging",
        "//cpp/platform/api:platform",
        "//cpp/platform/api:types",
        "//cpp/platform/base",
        "//cpp/platform/base:logging",
        "//cpp/platform/base:util",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "comm",
    srcs = [
        "ble.cc",
        "bluetooth_classic.cc",
        "file.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "webrtc.h",
        "wifi_lan.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//cpp/core:__subpackages__",
        "//cpp/platform/public:__pkg__",
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//internal/analytics:__subpackages__",
    ],
    deps = [
        ":logging",
        ":types",
        "//cpp/platform/api:comm",
        "//cpp/platform/api:platform",
        "//cpp/platform/base",
        "//cpp/platform/base:cancellation_flag",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//cpp/core:__subpackages__",
        "//cpp/platform:__subpackages__",
        "//internal/analytics:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
    ],
    deps = [
        "//cpp/platform/base:logging",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "ble_test.cc",
        "bluetooth_adapter_test.cc",
        "bluetooth_classic_test.cc",
        "cancelable_alarm_test.cc",
        "condition_variable_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "future_test.cc",
        "logging_test.cc",
        "multi_thread_executor_test.cc",
        "mutex_test.cc",
        "pipe_test.cc",
        "scheduled_executor_test.cc",
        "single_thread_executor_test.cc",
        "wifi_lan_test.cc",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    shard_count = 16,
    deps = [
        ":comm",
        ":logging",
        ":types",
        "//cpp/platform/base",
        "//cpp/platform/base:test_util",
        "//cpp/platform/impl/g3",  # build_cleaner: keep
        "@com_google_googletest//:gtest_main","@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_fake_binary(
    name = "thread_check_nocompile",
    srcs = ["thread_check_nocompile.cc"],
    deps = [
        ":types",
        "@com_google_absl//absl/time",
    ],
)

py_test(
    name = "thread_check_nocompile_test",
    size = "large",
    srcs = ["thread_check_nocompile_test.py"],
    data = ["thread_check_nocompile"],
    python_version = "PY3",
    srcs_version = "PY3",
    tags = ["non_compile_test"],
    deps = [
        "//testing/pybase",
        "//testing/pybase:fake_target_util",
    ],
)
