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
    name = "mediums",
    srcs = [
    ],
    hdrs = [
        "advertisement_data.h",
        "ble.h",
        "mediums.h",
    ],
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        "//internal/platform:comm",
        "//internal/platform:uuid",
        "//presence:types",
    ],
)

cc_test(
    name = "mediums_test",
    size = "small",
    srcs = [
        "ble_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ] + select({
        "@platforms//os:windows": [
            "//internal/platform/implementation/windows",
        ],
        "//conditions:default": [
            "//internal/platform/implementation/g3",
        ],
    }),
)
