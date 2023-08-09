cc_library(
    name = "crypto",
    srcs = [
        "fast_pair_decryption.cc",
        "fast_pair_encryption.cc",
        "fast_pair_message_type.cc",
    ],
    hdrs = [
        "decrypted_passkey.h",
        "decrypted_response.h",
        "fast_pair_decryption.h",
        "fast_pair_encryption.h",
        "fast_pair_key_pair.h",
        "fast_pair_message_type.h",
    ],
    visibility = [
        "//:__subpackages__",
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//internal/base:bluetooth_address",
        "//internal/platform:base",
        "//internal/platform:types",
        "@boringssl//:crypto",
        "@com_google_absl//absl/log:check",
    ],
)

cc_test(
    name = "decrypted_passkey_test",
    size = "small",
    srcs = [
        "decrypted_passkey_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":crypto",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "decrypted_response_test",
    size = "small",
    srcs = [
        "decrypted_response_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":crypto",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_key_pair_test",
    size = "small",
    srcs = [
        "fast_pair_key_pair_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":crypto",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_encryption_test",
    size = "small",
    srcs = [
        "fast_pair_encryption_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":crypto",
        "//fastpair/common",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_decryption_test",
    size = "small",
    srcs = [
        "fast_pair_decryption_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":crypto",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_message_type_test",
    size = "small",
    srcs = [
        "fast_pair_message_type_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":crypto",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
