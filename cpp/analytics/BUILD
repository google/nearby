licenses(["notice"])
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
    name = "analytics",
    srcs = [
        "analytics_recorder.cc",
    ],
    hdrs = [
        "analytics_recorder.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//core:__subpackages__",
        "//proto/analytics:__subpackages__",
    ],
    deps = [
        "//absl/container:btree",
        "//absl/time",
        "//core:core_types",
        "//core:event_logger",
        "//platform/base",
        "//platform/base:error_code_recorder",
        "//platform/public:logging",
        "//platform/public:types",
        "//proto:connections_enums_portable_proto",
        "//proto/analytics:connections_log_cc_proto",
        "//proto/errorcode:error_code_enums_portable_proto",
    ],
)

cc_test(
    name = "analytics_test",
    size = "small",
    srcs = [
        "analytics_recorder_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":analytics",
        "//testing/base/public:gunit_main",
        "//absl/time",
        "//platform/base:error_code_recorder",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
        "//proto:connections_enums_portable_proto",
        "//proto/analytics:connections_log_cc_proto",
    ],
)
