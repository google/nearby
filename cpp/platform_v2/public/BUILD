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

cc_library(
    name = "types",
    srcs = [
        "pipe.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "cancelable.h",
        "cancelable_alarm.h",
        "condition_variable.h",
        "count_down_latch.h",
        "crypto.h",
        "file.h",
        "future.h",
        "logging.h",
        "multi_thread_executor.h",
        "mutex.h",
        "mutex_lock.h",
        "pipe.h",
        "scheduled_executor.h",
        "settable_future.h",
        "single_thread_executor.h",
        "submittable_executor.h",
        "system_clock.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2/base:__pkg__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":logging",
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//platform_v2/base:logging",
        "//platform_v2/base:util",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/time",
    ],
)

cc_library(
    name = "comm",
    srcs = [
        "ble.cc",
        "bluetooth_classic.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "webrtc.h",
        "wifi_lan.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":logging",
        ":types",
        "//platform_v2/api:comm",
        "//platform_v2/api:platform",
        "//platform_v2/base",
        "//absl/container:flat_hash_map",
        "//absl/strings",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
    ],
    deps = [
        "//platform_v2/base:logging",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
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
    shard_count = 16,
    deps = [
        ":comm",
        ":logging",
        ":types",
        "//platform_v2/base",
        "//platform_v2/base:test_util",
        "//platform_v2/impl/g3",  # build_cleaner: keep
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/synchronization",
        "//absl/time",
    ],
)
