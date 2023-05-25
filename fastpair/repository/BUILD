licenses(["notice"])

cc_library(
    name = "repository",
    srcs = [
        "fast_pair_metadata_fetcher_impl.cc",
        "fast_pair_metadata_repository_impl.cc",
    ],
    hdrs = [
        "device_metadata.h",
        "fast_pair_metadata_fetcher.h",
        "fast_pair_metadata_fetcher_impl.h",
        "fast_pair_metadata_repository.h",
        "fast_pair_metadata_repository_impl.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//internal/network:types",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "device_repository",
    srcs = [
        "fast_pair_device_repository.cc",
    ],
    hdrs = [
        "fast_pair_device_repository.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/common",
        "//internal/platform:logging",
        "//internal/platform:types",
    ],
)

cc_test(
    name = "device_repository_test",
    srcs = [
        "fast_pair_device_repository_test.cc",
    ],
    deps = [
        ":device_repository",
        "//fastpair/common",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fake_fast_pair_metadata_repository",
    hdrs = ["fake_fast_pair_metadata_repository.h"],
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":repository",
        "//fastpair/proto:fastpair_cc_proto",
    ],
)

cc_test(
    name = "repository_test",
    srcs = [
        "fast_pair_metadata_fetcher_impl_test.cc",
        "fast_pair_metadata_repository_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":repository",
        "//fastpair/common",
        "//fastpair/internal/test:nearby_fastpair_test",
        "//fastpair/proto:fastpair_cc_proto",
        "//internal/network:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
