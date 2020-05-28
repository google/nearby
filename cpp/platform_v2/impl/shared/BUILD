cc_library(
    name = "posix_mutex",
    srcs = [
        "posix_mutex.cc",
    ],
    hdrs = [
        "posix_mutex.h",
    ],
    visibility = [
        "//platform_v2/impl:__subpackages__",
    ],
    deps = [
        "//platform_v2/api",
        "//platform_v2/base",
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
        "//platform_v2/impl:__subpackages__",
    ],
    deps = [
        ":posix_mutex",
        "//platform_v2/api",
        "//platform_v2/base",
    ],
)
