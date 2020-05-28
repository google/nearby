cc_library(
    name = "sample_platform",
    srcs = [
        "atomic_reference_impl.h",
        "sample_platform.cc",
        "settable_future_impl.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//core:__subpackages__",
        "//location/nearby/setup/core:__subpackages__",
    ],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/shared/sample:sample_wifi_medium",
        "//platform/port:string",
        "//absl/time",
        "//absl/types:any",
    ],
)
