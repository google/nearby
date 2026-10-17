load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Stand-ins for Google-internal analytics protos, recovered from the
# descriptors embedded in Quick Share for Windows (see file headers).
proto_library(
    name = "connections_log_proto",
    srcs = ["connections_log.proto"],
    deps = [
        "//proto:connections_enums_proto",
        "//proto/errorcode:error_code_enums_proto",
    ],
)

cc_proto_library(
    name = "connections_log_cc_proto",
    deps = [":connections_log_proto"],
)

proto_library(
    name = "sharing_log_proto",
    srcs = ["nearby_sharing_log.proto"],
    deps = [
        "//proto:sharing_enums_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "sharing_log_cc_proto",
    deps = [":sharing_log_proto"],
)
