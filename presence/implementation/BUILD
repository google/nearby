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
    name = "internal",
    srcs = ["advertisement_decoder.cc"],
    hdrs = [
        "advertisement_decoder.h",
        "broadcast_manager.h",
        "credential_manager.h",
        "credential_manager_impl.h",
        "mock_service_controller.h",
        "scan_manager.h",
        "service_controller.h",
        "service_controller_impl.h",
    ],
    visibility = [
        "//third_party/nearby/presence:__subpackages__",
    ],
    deps = [
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform/implementation:comm",
        "//third_party/nearby/presence:action_factory",
        "//third_party/nearby/presence:advertisement_factory",
        "//third_party/nearby/presence:credential",
        "//third_party/nearby/presence:types",
        "//third_party/nearby/presence/implementation/mediums",
        "//third_party/nearby/presence/proto:credential_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "advertisement_decoder_test",
    size = "small",
    srcs = ["advertisement_decoder_test.cc"],
    deps = [
        ":internal",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//third_party/nearby/presence:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
