licenses(["notice"])

cc_library(
    name = "dataparser",
    srcs = [
        "fast_pair_decoder.cc",
    ],
    hdrs = [
        "fast_pair_decoder.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "fast_pair_decoder_test",
    size = "small",
    srcs = [
        "fast_pair_decoder_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":dataparser",
        "//fastpair/testing",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)
