licenses(["notice"])

# Copyright 2023 Google LLC
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
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//internal/analytics:event_logger",
        "//internal/proto/analytics:fast_pair_log_cc_proto",
        "//proto:fast_pair_enums_cc_proto",
        "//third_party/protobuf:protobuf_lite",
    ],
)

cc_test(
    name = "analytics_recorder_test",
    srcs = ["analytics_recorder_test.cc"],
    deps = [
        ":analytics",
        "//internal/analytics:event_logger",
        "//internal/proto/analytics:fast_pair_log_cc_proto",
        "//proto:fast_pair_enums_cc_proto",
        "//third_party/protobuf:protobuf_lite",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
