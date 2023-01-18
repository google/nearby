licenses(["notice"])

cc_library(
    name = "types",
    hdrs = [
        "device_info.h",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/internal/api:platform",
        "//fastpair/internal/impl/g3:platform_g3",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "device_info",
    srcs = [
        "device_info_impl.cc",
    ],
    hdrs = [
        "device_info_impl.h",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        ":types",
        "//fastpair/internal/api:platform",
        "//fastpair/internal/impl/g3:platform_g3",
    ],
)
