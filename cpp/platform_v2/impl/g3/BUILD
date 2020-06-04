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
        "scheduled_executor.cc",
        "system_clock.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference_any.h",
        "condition_variable.h",
        "count_down_latch.h",
        "multi_thread_executor.h",
        "mutex.h",
        "pipe.h",
        "scheduled_executor.h",
        "settable_future_any.h",
        "single_thread_executor.h",
    ],
    visibility = [
        "//platform_v2/impl/g3:__pkg__",
    ],
    deps = [
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//platform_v2/base:util",
        "//platform_v2/impl/shared:posix_mutex",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//absl/types:any",
        "//thread",
    ],
)

cc_library(
    name = "comm",
    testonly = True,
    srcs = [
        "bluetooth_adapter.cc",
        "webrtc.cc",
    ],
    hdrs = [
        "bluetooth_adapter.h",
        "webrtc.h",
    ],
    visibility = [
        "//platform_v2/impl/g3:__pkg__",
    ],
    deps = [
        ":types",
        "//platform_v2/api:comm",
        "//platform_v2/base:test_util",
        "//absl/base:core_headers",
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
    visibility = [
        "//platform_v2/g3:__pkg__",
    ],
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
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
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
        "//platform_v2/impl/shared:file",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/time",
    ],
)
