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
    name = "g3",
    srcs = [
        "atomic_reference_impl.h",
        "platform.cc",
        "settable_future_impl.h",
        "system_clock_impl.h",
    ],
    visibility = [
        "//core:__subpackages__",
        "//platform:__subpackages__",
    ],
    deps = [
        "//platform:types",
        "//platform/api",
        "//platform/impl/shared:atomic_boolean",
        "//platform/impl/shared:file",
        "//platform/impl/shared:posix_condition_variable",
        "//platform/impl/shared:posix_lock",
        "//platform/port:string",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//absl/types:any",
    ],
)
