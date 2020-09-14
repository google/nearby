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
    testonly = True,
    srcs = [
        "log_message.cc",
        "scheduled_executor.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "condition_variable.h",
        "count_down_latch.h",
        "log_message.h",
        "multi_thread_executor.h",
        "mutex.h",
        "pipe.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//base",
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//platform_v2/base:util",
        "//platform_v2/impl/shared:posix_mutex",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//thread",
    ],
)

cc_library(
    name = "comm",
    testonly = True,
    srcs = [
        "ble.cc",
        "bluetooth_adapter.cc",
        "bluetooth_classic.cc",
        "webrtc.cc",
        "wifi_lan.cc",
    ],
    hdrs = [
        "ble.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "webrtc.h",
        "wifi_lan.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":types",
        "//platform_v2/api:comm",
        "//platform_v2/base",
        "//platform_v2/base:logging",
        "//platform_v2/base:test_util",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//absl/synchronization",
        "//webrtc/api:create_peerconnection_factory",  #buildcleaner: keep
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api/task_queue:default_task_queue_factory",
    ],
)

cc_library(
    name = "crypto",
    testonly = True,
    srcs = [
        "crypto.cc",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//absl/strings",
        "//openssl:crypto",
    ],
)

cc_library(
    name = "g3",
    testonly = True,
    srcs = [
        "platform.cc",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
    ],
    deps = [
        ":comm",
        ":crypto",  # build_cleaner: keep
        ":types",
        "//platform_v2/api:comm",
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/base:test_util",
        "//platform_v2/impl/shared:file",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/time",
    ],
)
