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
    name = "default",
    srcs = [
        "default_platform.cc",
    ],
    hdrs = [
        "default_condition_variable.h",
        "default_lock.h",
        "default_platform.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
    ],
    deps = [
        ":condition_variable",
        ":lock",
        "//platform:types",
        "//platform/api",
    ],
)

cc_library(
    name = "lock",
    srcs = ["default_lock.cc"],
    hdrs = ["default_lock.h"],
    visibility = [
        "//platform:__subpackages__",
    ],
    deps = ["//platform/api:lock"],
)

cc_library(
    name = "condition_variable",
    srcs = ["default_condition_variable.cc"],
    hdrs = ["default_condition_variable.h"],
    visibility = [
        "//platform:__subpackages__",
    ],
    deps = [
        ":lock",
        "//platform:types",
        "//platform/api:condition_variable",
    ],
)
