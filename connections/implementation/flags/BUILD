load("@rules_cc//cc:cc_library.bzl", "cc_library")

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
licenses(["notice"])

cc_library(
    name = "connections_flags",
    hdrs = [
        "nearby_connections_feature_flags.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//connections:partners",
        "//googlemac/iPhone/Shared/Identity/SmartSetup:__subpackages__",
        "//internal/platform:__subpackages__",
        "//internal/platform/implementation:__subpackages__",
        "//location/nearby/cpp:__subpackages__",
        "//location/nearby/sharing:__subpackages__",
        "//location/nearby/testing:__subpackages__",
        "//sharing:__subpackages__",
    ],
    deps = [
        "//internal/flags:flag_reader",
        "@com_google_absl//absl/strings",
    ],
)
