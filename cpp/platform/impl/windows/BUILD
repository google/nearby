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
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
    ],
    deps = [
        "//platform/api:types",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//platform/api:comm",
        "//platform/base",
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
        "//platform/api:types",
        "//platform/base",
        "//absl/strings",
        "//openssl:crypto",
    ],
)

cc_test(
    name = "impl_test",
    size = "small",
    srcs = [
        "crypto_test.cc",
    ],
    deps = [
        ":comm",
        ":crypto",
        ":types",
        "//platform/api:types",
        "//testing/base/public:gunit_main",
    ],
)
