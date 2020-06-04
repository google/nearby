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
    name = "webrtc",
    hdrs = [
        "webrtc_socket.cc",
        "webrtc_socket.h",
    ],
    deps = [
        "//platform:utils",
        "//platform/api",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "webrtc_test",
    srcs = ["webrtc_socket_test.cc"],
    deps = [
        ":webrtc",
        "//platform:types",
        "//platform/api",
        "//platform/impl/g3",  # buildcleaner: keep
        "//testing/base/public:gunit_main",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "peer_id",
    srcs = ["peer_id.cc"],
    hdrs = ["peer_id.h"],
    deps = [
        "//core/internal/mediums:utils",
        "//platform:types",
        "//platform/api",
        "//platform/port:string",
        "//absl/strings",
    ],
)

cc_library(
    name = "signaling_frames",
    srcs = ["signaling_frames.cc"],
    hdrs = ["signaling_frames.h"],
    deps = [
        ":peer_id",
        "//platform:types",
        "//location/nearby/mediums/proto:web_rtc_signaling_frames_cc_proto",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "peer_id_test",
    srcs = ["peer_id_test.cc"],
    deps = [
        ":peer_id",
        "//platform:types",
        "//platform/api",
        "//platform/impl/g3",  # buildcleaner: keep
        "//testing/base/public:gunit_main",
        "//absl/strings",
    ],
)

cc_test(
    name = "signaling_frames_test",
    srcs = ["signaling_frames_test.cc"],
    deps = [
        ":peer_id",
        ":signaling_frames",
        "//platform:types",
        "//platform/impl/g3",  # buildcleaner: keep
        "//net/proto2/public:proto2",
        "//testing/base/public:gunit_main",
        "//webrtc/pc:peerconnection",  # buildcleaner: keep
    ],
)
