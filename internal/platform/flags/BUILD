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
    name = "platform_flags",
    hdrs = [
        "nearby_platform_feature_flags.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//internal:__subpackages__",
        "//location/nearby/cpp:__subpackages__",
    ],
    deps = [
        "//internal/flags:flag_reader",
        "@com_google_absl//absl/strings",
    ],
)
