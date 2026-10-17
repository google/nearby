load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Stand-ins for the Nearby identity API protos, recovered from the descriptors
# embedded in Quick Share for Windows (see file headers).
proto_library(
    name = "resources_proto",
    srcs = ["resources.proto"],
    deps = ["@com_google_protobuf//:timestamp_proto"],
)

cc_proto_library(
    name = "resources_cc_proto",
    deps = [":resources_proto"],
)

proto_library(
    name = "rpcs_proto",
    srcs = ["rpcs.proto"],
    deps = [
        ":resources_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "rpcs_cc_proto",
    deps = [":rpcs_proto"],
)
