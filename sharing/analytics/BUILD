# Copyright 2024 Google LLC
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

load("@rules_cc//cc:cc_library.bzl", "cc_library")

licenses(["notice"])

cc_library(
    name = "analytics",
    hdrs = [
        "analytics_device_settings.h",
        "analytics_information.h",
        "analytics_recorder.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//proto:sharing_enums_cc_proto",
        "//sharing:attachments",
        "//sharing:types",
        "//sharing/common:enum",
        "//sharing/proto:enums_cc_proto",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)
