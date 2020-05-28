cc_library(
    name = "g3",
    srcs = [
        "atomic_boolean.h",
        "atomic_reference_any.h",
        "bluetooth_adapter.cc",
        "bluetooth_adapter.h",
        "condition_variable.h",
        "count_down_latch.h",
        "medium_environment.cc",
        "medium_environment.h",
        "multi_thread_executor.h",
        "mutex.h",
        "platform.cc",
        "scheduled_executor.cc",
        "scheduled_executor.h",
        "settable_future_any.h",
        "single_thread_executor.h",
        "system_clock.cc",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
    ],
    deps = [
        ":crypto",  # build_cleaner: keep
        "//platform_v2/api",
        "//platform_v2/base",
        "//platform_v2/impl/shared:posix_mutex",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//absl/time",
        "//absl/types:any",
        "//thread",
    ],
)

cc_library(
    name = "crypto",
    srcs = [
        "crypto.cc",
    ],
    visibility = [
        "//platform_v2/g3:__pkg__",
    ],
    deps = [
        "//platform_v2/api",
        "//platform_v2/base",
        "//absl/strings",
        "//openssl:crypto",
    ],
)
