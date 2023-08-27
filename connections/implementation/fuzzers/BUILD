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
load("//security/fuzzing/blaze:cc_fuzz_target.bzl", "cc_fuzz_target")

licenses(["notice"])

cc_fuzz_target(
    name = "offline_frames_fuzzer",
    srcs = ["offline_frames_fuzzer.cc"],
    componentid = 148515,
    copts = ["-DCORE_ADAPTER_DLL"],
    deps = [
        "//connections/implementation:internal",
        "//internal/platform:base",
        "//internal/platform/implementation/g3",
        "//security/fuzzing/blaze:default_init_google_for_cc_fuzz_target",
    ],
)
