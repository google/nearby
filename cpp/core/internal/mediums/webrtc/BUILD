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
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//cpp/core/internal:__subpackages__",
    ],
    deps = [
        ":data_types",
        "//cpp/core:core_types",
        "//cpp/core/internal/mediums:utils",
        "//cpp/platform/base",
        "//cpp/platform/public:comm",
        "//cpp/platform/public:logging",
        "//cpp/platform/public:types",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "//webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/pc:peerconnection",
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
    copts = ["-DCORE_ADAPTER_DLL"],
    visibility = [
        "//cpp/core/internal:__subpackages__",
    ],
    deps = [
        "//cpp/core:core_types",
        "//cpp/platform/base",
        "//cpp/platform/public:types",
        "//webrtc/api:libjingle_peerconnection_api",
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
        "//cpp/core/internal/mediums:utils",
        "//cpp/platform/base",
        "//cpp/platform/base:test_util",
        "//cpp/platform/impl/g3",  # buildcleaner: keep
        "//cpp/platform/public:comm",
        "//cpp/platform/public:types",
        "@com_google_googletest//:gtest_main","@com_github_protobuf_matchers//protobuf-matchers:protobuf-matchers",
        "@com_google_absl//absl/time",
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api:rtc_error",
        "//webrtc/api:scoped_refptr",
        "@com_google_protobuf//:protobuf",
    ],
)
