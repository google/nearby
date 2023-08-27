licenses(["notice"])

cc_library(
    name = "repository",
    srcs = [
        "fast_pair_repository.cc",
    ],
    hdrs = [
        "fast_pair_repository.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//internal/base:bluetooth_address",
        "//internal/crypto_cros",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "repository_impl",
    srcs = [
        "fast_pair_repository_impl.cc",
    ],
    hdrs = [
        "fast_pair_repository_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        ":repository",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/proto:proto_builder",
        "//fastpair/server_access",
        "//internal/base",
        "//internal/platform:types",
        "@com_google_absl//absl/status",
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
        "//internal/base",
        "//internal/platform:types",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_library(
    name = "test_support",
    srcs = ["fake_fast_pair_repository.cc"],
    hdrs = [
        "fake_fast_pair_repository.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":repository",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//internal/base",
        "//internal/platform:types",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "mocks",
    testonly = 1,
    hdrs = [
        "mock_fast_pair_repository.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":repository",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_for_library_testonly",
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
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_repository_test",
    srcs = [
        "fast_pair_repository_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":repository",
        "//fastpair/common",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "fast_pair_repository_impl_test",
    srcs = [
        "fast_pair_repository_impl_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":repository_impl",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/proto:proto_builder",
        "//fastpair/server_access:test_support",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
