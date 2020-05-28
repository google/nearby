cc_library(
    name = "g3",
    srcs = [
        "atomic_reference_impl.h",
        "platform.cc",
        "settable_future_impl.h",
        "system_clock_impl.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//platform:__subpackages__",
    ],
    deps = [
        "//platform:types",
        "//platform/api",
        "//platform/impl/shared:atomic_boolean",
        "//platform/impl/shared:posix_condition_variable",
        "//platform/impl/shared:posix_lock",
        "//platform/port:string",
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//absl/time",
        "//absl/types:any",
    ],
)
