licenses(["notice"])

cc_library(
    name = "server_access",
    srcs = [
        "fast_pair_metadata_downloader.cc",
        "fast_pair_metadata_downloader_impl.cc",
        "fast_pair_repository.cc",
        "fast_pair_repository_impl.cc",
    ],
    hdrs = [
        "fast_pair_metadata_downloader.h",
        "fast_pair_metadata_downloader_impl.h",
        "fast_pair_repository.h",
        "fast_pair_repository_impl.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository",
        "//internal/network:nearby_http_client",
        "//internal/network:types",
        "//internal/platform:logging",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "test_support",
    srcs = ["fake_fast_pair_repository.cc"],
    hdrs = ["fake_fast_pair_repository.h"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "server_access_test",
    srcs = [
        "fast_pair_metadata_downloader_impl_test.cc",
        "fast_pair_repository_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository",
        "//fastpair/repository:fake_fast_pair_metadata_repository",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
