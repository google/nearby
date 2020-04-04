cc_library(
    name = "default",
    srcs = [
        "default_platform.cc",
    ],
    hdrs = [
        "default_condition_variable.h",
        "default_lock.h",
        "default_platform.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
    ],
    deps = [
        ":condition_variable",
        ":lock",
        "//platform:types",
        "//platform/api",
    ],
)

cc_library(
    name = "lock",
    srcs = ["default_lock.cc"],
    hdrs = ["default_lock.h"],
    visibility = [
        "//platform:__subpackages__",
    ],
    deps = ["//platform/api:lock"],
)

cc_library(
    name = "condition_variable",
    srcs = ["default_condition_variable.cc"],
    hdrs = ["default_condition_variable.h"],
    visibility = [
        "//platform:__subpackages__",
    ],
    deps = [
        ":lock",
        "//platform:types",
        "//platform/api:condition_variable",
    ],
)
