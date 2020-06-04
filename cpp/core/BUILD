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
    name = "core",
    hdrs = [
        "core.cc",
        "core.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__pkg__",
        "//location/nearby/setup/core/internal:__pkg__",
    ],
    deps = [
        ":types",
        "//core/internal",
        "//platform:types",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "payload.cc",
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
    visibility = [
        "//core/internal:__pkg__",
        "//location/nearby/setup/core/internal:__pkg__",
    ],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
    ],
)

cc_library(
    name = "check_compilation",
    srcs = ["check_compilation.cc"],
    deps = [
        ":core",
        ":types",
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/g3",
        "//platform/impl/shared:file",
        "//platform/impl/shared/sample:sample_wifi_medium",
        "//platform/port:string",
    ],
)
