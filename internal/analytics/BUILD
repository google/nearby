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
    name = "event_logger",
    hdrs = [
        "event_logger.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//location/nearby/analytics/cpp:__subpackages__",
        "//location/nearby/cpp/experiments:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "//internal/proto/analytics:connections_log_cc_proto",
        "//internal/proto/analytics:fast_pair_log_cc_proto",
        "//sharing/proto/analytics:sharing_log_cc_proto",
    ],
)

cc_library(
    name = "mock_event_logger",
    testonly = True,
    hdrs = [
        "mock_event_logger.h",
        "sharing_log_matchers.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:public"],
    deps = [
        ":event_logger",
        "@com_google_googletest//:gtest_for_library_testonly",
        "@com_google_protobuf//:protobuf_lite",
    ],
)
