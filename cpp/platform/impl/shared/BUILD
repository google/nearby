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
    name = "posix_lock",
    srcs = [
        "posix_lock.cc",
    ],
    hdrs = [
        "posix_lock.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        "//platform/api",
    ],
)

cc_library(
    name = "posix_condition_variable",
    srcs = [
        "posix_condition_variable.cc",
    ],
    hdrs = [
        "posix_condition_variable.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        ":posix_lock",
        "//platform:types",
        "//platform/api:condition_variable",
    ],
)

cc_library(
    name = "atomic_boolean",
    hdrs = ["atomic_boolean_impl.h"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = ["//platform/api"],
)

cc_library(
    name = "file",
    srcs = ["file_impl.cc"],
    hdrs = ["file_impl.h"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        "//platform:types",
        "//platform/api",
    ],
)

cc_test(
    name = "file_test",
    timeout = "short",
    srcs = [
        "file_impl_test.cc",
    ],
    deps = [
        ":file",
        "//file/util:temp_path",
        "//testing/base/public:gunit_main",
        "//absl/strings",
    ],
)
