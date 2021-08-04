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
licenses(["notice"])

cc_library(
    name = "posix_mutex",
    srcs = [
        "posix_mutex.cc",
    ],
    hdrs = [
        "posix_mutex.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//platform/impl:__subpackages__",
    ],
    deps = ["//platform/api:types"],
)

cc_library(
    name = "posix_condition_variable",
    srcs = [
        "posix_condition_variable.cc",
    ],
    hdrs = [
        "posix_condition_variable.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        ":posix_mutex",
        "//platform/api:types",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        "//absl/strings",
        "//platform/api:types",
        "//platform/base",
    ],
)

cc_test(
    name = "file_test",
    srcs = ["file_test.cc"],
    deps = [
        ":file",
        "//file/util:temp_path",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//platform/base",
    ],
)
