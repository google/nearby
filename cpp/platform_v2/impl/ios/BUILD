objc_library(
    name = "types",
    srcs = [
        "log_message.mm",
        "scheduled_executor.mm",
    ],
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "condition_variable.h",
        "count_down_latch.h",
        "log_message.h",
        "multi_thread_executor.h",
        "mutex.h",
        "scheduled_executor.h",
        "single_thread_executor.h",
    ],
    visibility = [
        "//platform_v2/impl/ios:__pkg__",
    ],
    deps = [
        "//base",
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//platform_v2/base:util",
        "//platform_v2/impl/shared:posix_mutex",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//thread",
    ],
)

objc_library(
    name = "ios",
    srcs = [
        "platform.mm",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
    ],
    deps = [
        ":types",
        "//platform_v2/api:comm",
        "//platform_v2/api:platform",
        "//platform_v2/api:types",
        "//platform_v2/impl/shared:file",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/time",
    ],
)
