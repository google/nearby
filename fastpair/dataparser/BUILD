licenses(["notice"])

cc_library(
    name = "dataparser",
    srcs = [
        "fast_pair_data_parser.cc",
    ],
    hdrs = [
        "fast_pair_data_parser.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":decoder",
        "//fastpair/common",
        "//fastpair/crypto",
        "//internal/base:bluetooth_address",
        "//internal/platform:base",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "decoder",
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
    ],
)

cc_test(
    name = "fast_pair_data_parser_test",
    size = "small",
    srcs = [
        "fast_pair_data_parser_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":dataparser",
        "//fastpair/common",
        "//fastpair/crypto",
        "//fastpair/testing",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@boringssl//:crypto",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
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
        ":decoder",
        "//fastpair/testing",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
