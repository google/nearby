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
load("//tools/build_defs/cc:cc_fake_binary.bzl", "cc_fake_binary")

licenses(["notice"])

cc_library(
    name = "comm",
    srcs = [
        "ble.cc",
    ],
    hdrs = [
        "ble.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//third_party/nearby:__subpackages__",
        "//analytics:__subpackages__",
        "//core:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        "//absl/container:flat_hash_map",
        "//absl/strings",
        "//platform/api:comm",
        "//platform/api:platform",
        "//platform/base",
        "//platform/base:cancellation_flag",
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
        "//proto/connections:offline_wire_formats_portable_proto",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "ble_test.cc",
    ],
    copts = ["-DCORE_ADAPTER_DLL"],
    shard_count = 16,
    deps = [
        ":comm",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/synchronization",
        "//absl/time",
        "//platform/base",
        "//platform/base:test_util",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:logging",
        "//platform/public:types",
    ],
)
