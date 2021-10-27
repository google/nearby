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
    name = "webrtc",
    srcs = [
        "connection_flow.cc",
        "signaling_frames.cc",
    ],
    hdrs = [
        "connection_flow.h",
        "data_channel_listener.h",
        "local_ice_candidate_listener.h",
        "session_description_wrapper.h",
        "signaling_frames.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//core/internal:__subpackages__",
    ],
    deps = [
        ":data_types",
        "//absl/memory",
        "//absl/time",
        "//core:core_types",
        "//core/internal/mediums:utils",
        "//platform/base",
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        "//webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/pc:peerconnection",
    ],
)

cc_library(
    name = "data_types",
    srcs = [
        "webrtc_socket.cc",
    ],
    hdrs = [
        "webrtc_socket.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//core/internal:__subpackages__",
    ],
    deps = [
        "//core:core_types",
        "//platform/base",
        "//platform/public:types",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "webrtc_test",
    timeout = "short",
    srcs = [
        "connection_flow_test.cc",
        "signaling_frames_test.cc",
        "webrtc_socket_test.cc",
    ],
    tags = [
        "notsan",  # NOTE(b/139734036): known data race in usrsctplib.
        "requires-net:external",
    ],
    deps = [
        ":data_types",
        ":webrtc",
        "//net/proto2/compat/public:proto2",
        "//testing/base/public:gunit_main",
        "//absl/time",
        "//core/internal/mediums:utils",
        "//platform/base",
        "//platform/base:test_util",
        "//platform/impl/g3",  # buildcleaner: keep
        "//platform/public:comm",
        "//platform/public:types",
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api:rtc_error",
        "//webrtc/api:scoped_refptr",
    ],
)
