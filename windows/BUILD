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

load("//third_party/msvc:build_windows.bzl", "cc_windows_dll")

licenses(["notice"])

# Build with --config=lexan
cc_windows_dll(
    name = "core_adapter",
    srcs = ["core_adapter.cc"],
    hdrs = ["core_adapter.h"],
    copts = ["-DCORE_ADAPTER_BUILD_DLL"],
    defines = ["CORE_ADAPTER_DLL"],
    tags = ["windows-dll"],
    deps = [
        "//absl/strings",
        "//core",
        "//platform/impl/windows",
        "//webrtc/api:create_peerconnection_factory",
    ],
)
