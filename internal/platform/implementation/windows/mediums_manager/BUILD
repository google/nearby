# Copyright 2022 Google LLC
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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "types",
    hdrs = ["mediums_manager.h"],
    deps = [
        "//internal/platform:base",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mediums_manager",
    srcs = [
        "mediums_api.cc",
        "mediums_manager_impl.cc",
    ],
    hdrs = [
        "mediums_manager_impl.h",
    ],
    copts = [
        "-Ithird_party/nearby/internal/platform/implementation/windows/generated",
    ],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    deps = [
        ":types",
        "//internal/platform:logging",
        "//internal/platform/implementation/windows:mediums_api",
        "//internal/platform/implementation/windows/generated:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)
