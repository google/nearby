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

objc_library(
    name = "types",
    srcs = [
        "log_message.mm",
        "scheduled_executor.mm",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "condition_variable.h",
        "count_down_latch.h",
        "log_message.h",
        "multi_thread_executor.h",
        "mutex.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
    ],
    visibility = [
        "//platform/impl/ios:__pkg__",
    ],
    deps = [
        "//base",
        "//platform/api:platform",
        "//platform/api:types",
        "//platform/base",
        "//platform/base:util",
        "//platform/impl/shared:posix_mutex",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//thread",
    ],
)

objc_library(
    name = "ios",
    srcs = [
        "platform.mm",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform:__subpackages__",
    ],
    deps = [
        ":types",
        "//platform/api:comm",
        "//platform/api:platform",
        "//platform/api:types",
        "//platform/impl/shared:file",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/time",
    ],
)
