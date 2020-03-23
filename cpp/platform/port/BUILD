cc_library(
    name = "config",
    hdrs = [
        "config.h",
    ],
    visibility = [
        "//visibility:private",
    ],
)

cc_library(
    name = "string",
    hdrs = [
        "string.h",
    ],
    visibility = [
        "//core:__subpackages__",
        "//platform:__subpackages__",
        "//location/nearby/setup/core:__subpackages__",
    ],
    deps = [
        ":config",
    ],
)

cc_library(
    name = "down_cast",
    hdrs = [
        "down_cast.h",
    ],
    visibility = [
        "//core:__subpackages__",
        "//platform:__subpackages__",
    ],
    deps = [
        ":config",
    ],
)
