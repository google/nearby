licenses(["notice"])

cc_library(
    name = "bluetooth_address",
    srcs = [
        "bluetooth_address.cc",
    ],
    hdrs = [
        "bluetooth_address.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "base_test",
    size = "small",
    timeout = "short",
    srcs = [
        "bluetooth_address_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":bluetooth_address",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
