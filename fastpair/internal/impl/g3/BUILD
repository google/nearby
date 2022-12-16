licenses(["notice"])

cc_library(
    name = "platform_g3",
    srcs = [
        "fast_pair_platform.cc",
    ],
    hdrs = [
        "device_info.h",
        "timer.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/internal/api:platform",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "g3_test",
    size = "small",
    timeout = "short",
    srcs = [
        "device_info_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":platform_g3",
        "//fastpair/internal/api:platform",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)
