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
    name = "core",
    srcs = [
        "core.cc",
    ],
    hdrs = [
        "core.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//third_party/nearby_connections/windows:__subpackages__",
    ],
    deps = [
        ":core_types",
        "//absl/strings",
        "//absl/time",
        "//absl/types:span",
        "//core/internal",
        "//platform/base",
        "//platform/public:logging",
        "//platform/public:types",
    ],
)

cc_library(
    name = "core_types",
    srcs = [
        "strategy.cc",
    ],
    hdrs = [
        "listeners.h",
        "options.h",
        "params.h",
        "payload.h",
        "status.h",
        "strategy.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
    ],
    deps = [
        "//absl/strings",
        "//absl/types:variant",
        "//platform/base",
        "//platform/base:util",
        "//platform/public:types",
        "//proto:connections_enums_portable_proto",
    ],
)

cc_test(
    name = "core_test",
    size = "small",
    srcs = [
        "core_test.cc",
        "listeners_test.cc",
        "payload_test.cc",
        "status_test.cc",
        "strategy_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":core",
        ":core_types",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/time",
        "//absl/types:variant",
        "//core/internal",
        "//core/internal:internal_test",
        "//platform/base",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:logging",
        "//platform/public:types",
    ],
)
