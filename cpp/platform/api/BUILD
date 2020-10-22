cc_library(
    name = "types",
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "cancelable.h",
        "condition_variable.h",
        "count_down_latch.h",
        "crypto.h",
        "executor.h",
        "future.h",
        "input_file.h",
        "listenable_future.h",
        "log_message.h",
        "mutex.h",
        "output_file.h",
        "scheduled_executor.h",
        "settable_future.h",
        "submittable_executor.h",
        "system_clock.h",
    ],
    visibility = [
        "//platform/base:__pkg__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        "//platform/base",
        "//absl/base:core_headers",
        "//absl/strings",
        "//absl/time",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "ble.h",
        "ble_v2.h",
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "server_sync.h",
        "webrtc.h",
        "wifi.h",
        "wifi_lan.h",
    ],
    visibility = [
        "//platform/base:__pkg__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        "//proto/connections:offline_wire_formats_portable_proto",
        "//platform/base",
        "//absl/strings",
        "//absl/types:optional",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "platform",
    hdrs = [
        "platform.h",
    ],
    visibility = [
        "//platform/base:__pkg__",
        "//platform/impl:__subpackages__",
        "//platform/public:__pkg__",
    ],
    deps = [
        ":comm",
        ":types",
        "//platform/base",
        "//absl/strings",
    ],
)
