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
        "connection_attempt_metadata_params.h",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//connections:__subpackages__",
        "//internal/proto/analytics:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//connections:event_logger",
        "//internal/platform:base",
        "//internal/platform:error_code_recorder",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/proto/analytics:connections_log_cc_proto",
        "//proto:connections_enums_cc_proto",
        "//proto/errorcode:error_code_enums_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/time",
    ],
)
