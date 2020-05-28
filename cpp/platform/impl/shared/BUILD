cc_library(
    name = "posix_lock",
    srcs = [
        "posix_lock.cc",
    ],
    hdrs = [
        "posix_lock.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        "//platform/api",
    ],
)

cc_library(
    name = "posix_condition_variable",
    srcs = [
        "posix_condition_variable.cc",
    ],
    hdrs = [
        "posix_condition_variable.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        ":posix_lock",
        "//platform:types",
        "//platform/api:condition_variable",
    ],
)

cc_library(
    name = "atomic_boolean",
    hdrs = ["atomic_boolean_impl.h"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = ["//platform/api"],
)
