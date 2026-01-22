licenses(["notice"])

cc_library(
    name = "server_access",
    srcs = [
        "fast_pair_metadata_downloader.cc",
        "fast_pair_metadata_downloader_impl.cc",
        "fast_pair_metadata_manager.cc",
        "fast_pair_metadata_manager_impl.cc",
    ],
    hdrs = [
        "fast_pair_metadata_downloader.h",
        "fast_pair_metadata_downloader_impl.h",
        "fast_pair_metadata_manager.h",
        "fast_pair_metadata_manager_impl.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository",
        "//internal/base",
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "server_access_test",
    srcs = [
        "fast_pair_metadata_downloader_impl_test.cc",
        "fast_pair_metadata_manager_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":server_access",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository:fake_fast_pair_metadata_repository",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
