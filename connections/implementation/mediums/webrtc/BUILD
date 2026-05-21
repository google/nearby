load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

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
    hdrs = [
        "data_channel_listener.h",
        "local_ice_candidate_listener.h",
        "session_description_wrapper.h",
    ],
    copts = ["-DNO_WEBRTC"],
    deps = [
        "//connections/implementation/mediums:webrtc_socket",
        "//internal/platform:base",
        # "//third_party/webrtc/files/stable/webrtc/api:jsep",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_library(
    name = "connection_flow",
    srcs = ["connection_flow.cc"],
    hdrs = ["connection_flow.h"],
    deps = [
        ":webrtc",
        ":webrtc_medium",
        ":webrtc_socket_impl",
        "//connections/implementation/mediums:webrtc_socket",
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:logging",
        "//internal/platform:types",
        # "//third_party/webrtc/files/stable/webrtc/api:data_channel_interface",
        # "//third_party/webrtc/files/stable/webrtc/api:jsep",
        # "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
        # "//third_party/webrtc/files/stable/webrtc/api:rtc_error",
        # "//third_party/webrtc/files/stable/webrtc/api:scoped_refptr",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:network_constants",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:refcount",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:threading",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "signaling_frames",
    srcs = ["signaling_frames.cc"],
    hdrs = ["signaling_frames.h"],
    deps = [
        "//connections/implementation/mediums:webrtc_peer_id",
        "//internal/platform:base",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        # "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "webrtc_socket_impl",
    srcs = ["webrtc_socket_impl.cc"],
    hdrs = ["webrtc_socket_impl.h"],
    deps = [
        "//connections/implementation/mediums:webrtc_socket",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        # "//third_party/webrtc/files/stable/webrtc/api:data_channel_interface",
        # "//third_party/webrtc/files/stable/webrtc/api:scoped_refptr",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "webrtc_medium",
    hdrs = ["webrtc.h"],
    deps = [
        "//internal/platform:base",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        # "//third_party/webrtc/files/stable/webrtc/api:libjingle_peerconnection_api",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:network_constants",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "webrtc_impl",
    srcs = [
        "webrtc_bwu_handler.cc",
        "webrtc_impl.cc",
    ],
    hdrs = [
        "webrtc_bwu_handler.h",
        "webrtc_impl.h",
    ],
    visibility = [
        "//connections/implementation/mediums:__pkg__",
    ],
    deps = [
        ":connection_flow",
        ":signaling_frames",
        ":webrtc",
        ":webrtc_medium",
        "//connections:core_types",
        "//connections/implementation:bwu_handler",
        "//connections/implementation:client_proxy",
        "//connections/implementation:endpoint_channel",
        "//connections/implementation:offline_frames",
        "//connections/implementation/mediums",
        "//connections/implementation/mediums:webrtc",
        "//connections/implementation/mediums:webrtc_peer_id",
        "//connections/implementation/mediums:webrtc_socket",
        "//connections/implementation/proto:offline_wire_formats_cc_proto",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//proto/mediums:web_rtc_signaling_frames_cc_proto",
        # "//third_party/webrtc/files/stable/webrtc/api:jsep",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:network_constants",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "fake_webrtc",
    testonly = True,
    srcs = ["fake_webrtc.cc"],
    hdrs = ["fake_webrtc.h"],
    deps = [
        ":webrtc_medium",
        "//internal/platform:cancellation_flag",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "webrtc_test",
    timeout = "short",
    srcs = [
        "connection_flow_test.cc",
        "signaling_frames_test.cc",
        "webrtc_impl_test.cc",
        "webrtc_socket_impl_test.cc",
    ],
    shard_count = 16,
    tags = [
        "requires-net:external",
    ],
    deps = [
        ":connection_flow",
        ":fake_webrtc",
        ":signaling_frames",
        ":webrtc",
        ":webrtc_impl",
        ":webrtc_medium",
        ":webrtc_socket_impl",
        "//connections/implementation/mediums:webrtc",
        "//connections/implementation/mediums:webrtc_peer_id",
        "//connections/implementation/mediums:webrtc_socket",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation:platform_impl",
        # "//third_party/webrtc/files/stable/webrtc/api:data_channel_interface",
        # "//third_party/webrtc/files/stable/webrtc/api:jsep",
        # "//third_party/webrtc/files/stable/webrtc/api:scoped_refptr",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:network_constants",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:refcount",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
