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
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform/base:__pkg__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":logging",
        "//platform/api:platform",
        "//platform/api:types",
        "//platform/base",
        "//platform/base:logging",
        "//platform/base:util",
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
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":logging",
        ":types",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform/api:comm",
        "//platform/api:platform",
        "//platform/base",
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
        "//core:__subpackages__",
        "//platform:__subpackages__",
    ],
    deps = [
        "//platform/base:logging",
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
        "//platform/base",
        "//platform/base:test_util",
        "//platform/impl/g3",  # build_cleaner: keep
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/synchronization",
        "//absl/time",
    ],
)
