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
        "//core/internal:__subpackages__",
    ],
    deps = [
        "//core:core_types",
        "//core/internal/mediums:utils",
        "//platform/base",
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
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
        "//platform/base",
        "//platform/base:test_util",
        "//platform/impl/g3",  # buildcleaner: keep
        "//platform/public:comm",
        "//platform/public:types",
        "//net/proto2/public:proto2",
        "//testing/base/public:gunit_main",
        "//absl/time",
        "//webrtc/api:libjingle_peerconnection_api",
        "//webrtc/api:rtc_error",
        "//webrtc/api:scoped_refptr",
    ],
)
