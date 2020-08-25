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
    name = "posix_mutex",
    srcs = [
        "posix_mutex.cc",
    ],
    hdrs = [
        "posix_mutex.h",
    ],
    visibility = [
        "//platform_v2/impl:__subpackages__",
    ],
    deps = ["//platform_v2/api:types"],
)

cc_library(
    name = "posix_condition_variable",
    srcs = [
        "posix_condition_variable.cc",
    ],
    hdrs = [
        "posix_condition_variable.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        ":posix_mutex",
        "//platform_v2/api:types",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    visibility = [
        "//platform_v2/impl:__subpackages__",
    ],
    deps = [
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//absl/strings",
    ],
)

cc_test(
    name = "file_test",
    srcs = ["file_test.cc"],
    deps = [
        ":file",
        "//file/util:temp_path",
        "//platform_v2/base",
        "//testing/base/public:gunit_main",
        "//absl/strings",
    ],
)
