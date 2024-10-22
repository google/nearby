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

cc_test(
    name = "offline_frames_fuzzer",
    srcs = ["offline_frames_fuzzer.cc"],
    linkopts = [
        "-Wl,--warn-backrefs-exclude=*third_party/nearby/internal/platform/implementation/g3/_objs*",
    ],
    tags = ["componentid:148515"],
    deps = [
        "//connections/implementation:internal",
        "//internal/platform:base",
        "//internal/platform/implementation/g3",
        "//testing/fuzzing:fuzztest",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
