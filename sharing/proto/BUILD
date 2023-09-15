load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

proto_library(
    name = "share_proto",
    srcs = [
        "certificate_rpc.proto",
        "contact_rpc.proto",
        "device_rpc.proto",
        "encrypted_metadata.proto",
        "enums.proto",
        "field_mask.proto",
        "rpc_resources.proto",
        "settings_observer_data.proto",
        "timestamp.proto",
        "wire_format.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//proto:sharing_enums_proto",
    ],
)

proto_library(
    name = "enums_proto",
    srcs = ["enums.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "share_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":share_proto"],
)
