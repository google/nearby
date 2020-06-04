cc_library(
    name = "types",
    srcs = [
        "pipe.cc",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "cancelable.h",
        "cancelable_alarm.h",
        "condition_variable.h",
        "count_down_latch.h",
        "crypto.h",
        "file.h",
        "future.h",
        "multi_thread_executor.h",
        "mutex.h",
        "mutex_lock.h",
        "pipe.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
        "submittable_executor.h",
        "system_clock.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2/base:__pkg__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":logging",
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//platform_v2/base:util",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/time",
        "//absl/types:any",
    ],
)

cc_library(
    name = "comm",
    srcs = [
        "bluetooth_classic.cc",
    ],
    hdrs = [
        "bluetooth_adapter.h",
        "bluetooth_classic.h",
        "webrtc.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":logging",
        ":types",
        "//platform_v2/api:comm",
        "//platform_v2/api:platform",
        "//platform_v2/base",
        "//absl/container:flat_hash_map",
        "//absl/strings",
        "//webrtc/api:libjingle_peerconnection_api",
    ],
)

cc_library(
    name = "logging",
    hdrs = [
        "logging.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
    ],
    deps = [
        "//platform_v2/base:logging",
    ],
)

cc_test(
    name = "public_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "bluetooth_adapter_test.cc",
        "bluetooth_classic_test.cc",
        "count_down_latch_test.cc",
        "crypto_test.cc",
        "future_test.cc",
        "logging_test.cc",
        "multi_thread_executor_test.cc",
        "mutex_test.cc",
        "pipe_test.cc",
        "scheduled_executor_test.cc",
        "single_thread_executor_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":comm",
        ":logging",
        ":types",
        "//platform_v2/base",
        "//platform_v2/base:test_util",
        "//platform_v2/impl/g3",  # build_cleaner: keep
        "//testing/base/public:gunit_main",
        "//absl/synchronization",
        "//absl/time",
    ],
)
