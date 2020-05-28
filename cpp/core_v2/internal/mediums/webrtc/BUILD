cc_library(
    name = "webrtc",
    srcs = [
        "webrtc_socket.cc",
    ],
    hdrs = [
        "webrtc_socket.h",
    ],
    deps = [
        "//core_v2:core_types",
        "//platform_v2/base",
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "webrtc_test",
    srcs = ["webrtc_socket_test.cc"],
    deps = [
        ":webrtc",
        "//platform_v2/base",
        "//platform_v2/impl/g3",  # buildcleaner: keep
        "//testing/base/public:gunit_main",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "peer_id_test",
    srcs = ["peer_id_test.cc"],
    deps = [
        ":peer_id",
        "//platform_v2/base",
        "//platform_v2/impl/g3",  #buildcleaner: keep
        "//platform_v2/public",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "signaling_frames_test",
    srcs = ["signaling_frames_test.cc"],
    deps = [
        ":peer_id",
        ":signaling_frames",
        "//platform_v2/impl/g3",  # buildcleaner: keep
        "//net/proto2/public:proto2",
        "//testing/base/public:gunit_main",
        "//webrtc/pc:peerconnection",  # buildcleaner: keep
    ],
)

cc_library(
    name = "peer_id",
    srcs = ["peer_id.cc"],
    hdrs = ["peer_id.h"],
    deps = [
        "//core_v2/internal/mediums:utils",
        "//platform_v2/base",
        "//absl/strings",
    ],
)

cc_library(
    name = "signaling_frames",
    srcs = ["signaling_frames.cc"],
    hdrs = ["signaling_frames.h"],
    deps = [
        ":peer_id",
        "//platform_v2/base",
        "//location/nearby/mediums/proto:web_rtc_signaling_frames_cc_proto",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)
