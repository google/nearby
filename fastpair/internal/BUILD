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
        "//fastpair/internal/mediums",
        "//fastpair/pairing",
        "//fastpair/repository:device_repository",
        "//fastpair/scanning:scanner",
        "//internal/platform:types",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "fast_pair_seeker_impl_test",
    size = "small",
    srcs = [
        "fast_pair_seeker_impl_test.cc",
    ],
    deps = [
        ":internal",
        "//fastpair/message_stream:fake_provider",
        "//fastpair/server_access:test_support",
        "//internal/platform:test_util",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
