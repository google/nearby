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
    name = "authentication_transport_interface",
    hdrs = [
        "authentication_transport.h",
    ],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "device",
    hdrs = [
        "device.h",
        "device_provider.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":authentication_status",
        ":authentication_transport_interface",
        "//internal/platform:connection_info",
        "//internal/platform:types",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "authentication_status",
    hdrs = [
        "authentication_status.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//presence:__subpackages__",
        "//sharing:__subpackages__",
    ],
)

cc_library(
    name = "test_support",
    testonly = 1,
    srcs = [
        "fake_device_provider.cc",
    ],
    hdrs = [
        "fake_device_provider.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [
        ":authentication_status",
        ":authentication_transport_interface",
        ":device",
        "@com_google_absl//absl/strings:string_view",
    ],
)
