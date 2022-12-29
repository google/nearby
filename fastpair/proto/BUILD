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
