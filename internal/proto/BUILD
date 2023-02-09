proto_library(
    name = "credential_proto",
    srcs = ["credential.proto"],
    visibility = ["//location/nearby/presence/proto:__pkg__"],
)

cc_proto_library(
    name = "credential_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":credential_proto"],
)

proto_library(
    name = "local_credential_proto",
    srcs = ["local_credential.proto"],
    visibility = ["//location/nearby/presence/proto:__pkg__"],
    deps = [":credential_proto"],
)

cc_proto_library(
    name = "local_credential_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":local_credential_proto"],
)

proto_library(
    name = "metadata_proto",
    srcs = ["metadata.proto"],
    visibility = ["//location/nearby/presence/proto:__pkg__"],
)

cc_proto_library(
    name = "metadata_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":metadata_proto"],
)
