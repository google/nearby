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
    name = "util",
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
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:json",
    ],
)
