licenses(["notice"])

cc_library(
    name = "internal",
    srcs = ["fast_pair_seeker_impl.cc"],
    hdrs = [
        "fast_pair_seeker_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair:fast_pair_events",
        "//fastpair:fast_pair_seeker",
        "//fastpair/repository:device_repository",
        "//internal/platform:types",
        "@com_google_absl//absl/status",
    ],
)
