load("@rules_cc//cc:defs.bzl", "cc_proto_library")

proto_library(
    name = "presence_frame_proto",
    srcs = ["presence_frame.proto"],
)

cc_proto_library(
    name = "presence_frame_cc_proto",
    visibility = [
        "//presence:__subpackages__",
    ],
    deps = [":presence_frame_proto"],
)
