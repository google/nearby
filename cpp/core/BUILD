cc_library(
    name = "core",
    hdrs = [
        "core.cc",
        "core.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__pkg__",
        "//location/nearby/setup/core/internal:__pkg__",
    ],
    deps = [
        ":types",
        "//core/internal",
        "//platform:types",
    ],
)

cc_library(
    name = "types",
    srcs = [
        "payload.cc",
        "strategy.cc",
    ],
    hdrs = [
        "listeners.h",
        "options.h",
        "params.h",
        "payload.h",
        "status.h",
        "strategy.h",
    ],
    visibility = [
        "//core/internal:__pkg__",
        "//location/nearby/setup/core/internal:__pkg__",
    ],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/port:string",
    ],
)

cc_library(
    name = "check_compilation",
    srcs = ["check_compilation.cc"],
    deps = [
        ":core",
        ":types",
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/g3",
        "//platform/impl/shared/sample:sample_wifi_medium",
        "//platform/port:string",
    ],
)
