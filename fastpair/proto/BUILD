load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

proto_library(
    name = "fastpair_proto",
    srcs = ["fastpair_rpcs.proto"],
)

cc_proto_library(
    name = "fastpair_cc_proto",
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [":fastpair_proto"],
)
