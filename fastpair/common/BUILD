licenses(["notice"])

cc_library(
    name = "common",
    srcs = [
        "fast_pair_device.cc",
        "fast_pair_http_result.cc",
        "pair_failure.cc",
        "protocol.cc",
    ],
    hdrs = [
        "constant.h",
        "context.h",
        "context_impl.h",
        "fast_pair_device.h",
        "fast_pair_http_result.h",
        "pair_failure.h",
        "protocol.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "fast_pair_device_test",
    size = "small",
    srcs = [
        "fast_pair_device_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":common",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

# cc_test(
#     name = "context_test",
#     size = "small",
#     srcs = [
#         "context_test.cc",
#     ],
#     shard_count = 16,
#     deps = [
#         ":common",
#         "//testing/base/public:gunit_main",
#         "//internal/platform/implementation/g3",
#     ],
# )
