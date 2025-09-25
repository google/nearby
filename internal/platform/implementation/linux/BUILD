cc_library(
    name = "linux_platform_impl",
    srcs = [
        "device_info.cc"
    ],
    hdrs = [
        "device_info.h"
    ],
    deps = [
        "@sdbus_cpp//:sdbus_cpp",
        "@sdbus_cpp//:libsystemd",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings"
    ],
    visibility = ["//visibility:public"],
)
