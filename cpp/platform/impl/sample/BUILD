cc_library(
    name = "sample",
    srcs = [
        "sample_wifi_medium.cc",
        "sample_wifi_medium.h",
    ],
    hdrs = ["sample_platform.h"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//location/nearby/setup/core:__subpackages__",
    ],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
        "//absl/time",
    ],
)
