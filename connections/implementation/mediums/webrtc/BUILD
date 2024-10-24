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
    copts = [
        "-DCORE_ADAPTER_DLL",
        "-DNO_WEBRTC",
    ],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        ":data_types",
        "//connections:core_types",
        "//connections/implementation/mediums:utils",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        # TODO: Support WebRTC
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "data_types",
    srcs = [
        "webrtc_socket_impl.cc",
    ],
    hdrs = [
        "webrtc_socket_impl.h",
    ],
    copts = [
        "-DCORE_ADAPTER_DLL",
        "-DNO_WEBRTC",
    ],
    visibility = [
        "//connections/implementation:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//internal/platform:base",
        "//internal/platform:types",
    ],
)

cc_test(
    name = "webrtc_test",
    timeout = "short",
    srcs = [
        "connection_flow_test.cc",
        "signaling_frames_test.cc",
        "webrtc_socket_impl_test.cc",
    ],
    tags = [
        "notsan",  # NOTE(b/139734036): known data race in usrsctplib.
        "requires-net:external",
    ],
    deps = [
        ":data_types",
        ":webrtc",
        "//connections/implementation/mediums:utils",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # buildcleaner: keep
        "//third_party/protobuf",
        "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/api:scoped_refptr",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
