# Copyright 2022 Google LLC
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

package_group(
    name = "nearby_fastpair",
    packages = [
        "//fastpair/...",
    ],
)

cc_library(
    name = "fastpair_controller",
    srcs = [],
    hdrs = [
        "fast_pair_controller.h",
    ],
    copts = ["-Ithird_party"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
    ],
)
