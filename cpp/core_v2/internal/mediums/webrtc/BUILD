cc_library(
    name = "webrtc",
    srcs = [
        "connection_flow.cc",
        "data_channel_observer_impl.cc",
        "peer_connection_observer_impl.cc",
        "peer_id.cc",
        "signaling_frames.cc",
        "webrtc_socket.cc",
    ],
    hdrs = [
        "connection_flow.h",
        "data_channel_listener.h",
        "data_channel_observer_impl.h",
        "local_ice_candidate_listener.h",
        "peer_connection_observer_impl.h",
        "peer_id.h",
        "session_description_wrapper.h",
        "signaling_frames.h",
        "webrtc_socket.h",
        "webrtc_socket_wrapper.h",
    ],
    visibility = [
        "//core_v2/internal:__subpackages__",
    ],
    deps = [
        "//core_v2:core_types",
        "//core_v2/internal/mediums:utils",
        "//platform_v2/base",
        "//platform_v2/public:comm",
        "//platform_v2/public:logging",
        "//platform_v2/public:types",
        "//location/nearby/mediums/proto:web_rtc_signaling_frames_cc_proto",
        "//absl/memory",
        "//absl/strings",
        "//absl/time",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_test(
    name = "webrtc_test",
    srcs = [
        "connection_flow_test.cc",
        "peer_id_test.cc",
        "signaling_frames_test.cc",
        "webrtc_socket_test.cc",
    ],
    deps = [
        ":webrtc",
        "//platform_v2/base",
        "//platform_v2/impl/g3",  # buildcleaner: keep
        "//platform_v2/public:comm",
        "//platform_v2/public:types",
        "//net/proto2/public:proto2",
        "//testing/base/public:gunit_main",
        "//absl/time",
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api:rtc_error",
        "//webrtc/api:scoped_refptr",
    ],
)
