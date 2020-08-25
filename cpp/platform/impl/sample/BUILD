cc_library(
    name = "sample_platform",
    srcs = [
        "atomic_reference_impl.h",
        "sample_platform.cc",
        "settable_future_impl.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//platform:types",
        "//platform:utils",
        "//platform/api",
        "//platform/impl/shared:file",
        "//platform/impl/shared/sample:sample_wifi_medium",
        "//platform/port:string",
        "//absl/time",
        "//absl/types:any",
    ],
)
