load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

proto_library(
    name = "fastpair_proto",
    srcs = [
        "cache.proto",
        "data.proto",
        "enum.proto",
        "fast_pair_string.proto",
        "fastpair_rpcs.proto",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
)

cc_proto_library(
    name = "fastpair_cc_proto",
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [":fastpair_proto"],
)

cc_library(
    name = "proto_to_json",
    srcs = [
        "proto_to_json.cc",
    ],
    hdrs = [
        "proto_to_json.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:public"],
    deps = [
        ":fastpair_cc_proto",
        "//fastpair/common",
        "//internal/platform:types",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "proto_builder",
    srcs = [
        "proto_builder.cc",
    ],
    hdrs = [
        "proto_builder.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:public"],
    deps = [
        ":fastpair_cc_proto",
        "//fastpair/common",
        "//fastpair/repository",
        "//internal/platform:types",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "proto_builder_test",
    srcs = [
        "proto_builder_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":fastpair_cc_proto",
        ":proto_builder",
        "//fastpair/common",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
