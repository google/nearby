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

licenses(["notice"])

cc_library(
    name = "analytics",
    srcs = [
        "analytics_recorder.cc",
    ],
    hdrs = [
        "analytics_device_settings.h",
        "analytics_information.h",
        "analytics_recorder.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/analytics:event_logger",
        "//proto:sharing_enums_cc_proto",
        "//sharing:attachments",
        "//sharing:types",
        "//sharing/common:enum",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto/analytics:sharing_log_cc_proto",
        "@com_google_absl//absl/random",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_test(
    name = "analytics_test",
    srcs = ["analytics_recorder_test.cc"],
    deps = [
        ":analytics",
        "//internal/analytics:mock_event_logger",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//proto:sharing_enums_cc_proto",
        "//sharing:attachments",
        "//sharing:types",
        "//sharing/common:enum",
        "//sharing/proto:enums_cc_proto",
        "//sharing/proto:wire_format_cc_proto",
        "//sharing/proto/analytics:sharing_log_cc_proto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:protobuf_lite",
    ],
)
