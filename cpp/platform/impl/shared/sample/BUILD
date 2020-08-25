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
    name = "sample_wifi_medium",
    srcs = [
        "sample_wifi_medium.cc",
    ],
    hdrs = [
        "sample_wifi_medium.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
        "//absl/time",
    ],
)
