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
        "//platform_v2/base:__pkg__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
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
        "//platform_v2/base:__pkg__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
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
        "//platform_v2/base:__pkg__",
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":comm",
        ":types",
        "//platform_v2/base",
        "//absl/strings",
    ],
)
