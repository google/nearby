load("@rules_cc//cc:defs.bzl", "cc_proto_library")
# NOTE: gRPC libraries not needed.

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
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
)

proto_library(
    name = "tachyon_proto",
    srcs = [
        "duration.proto",
        "ice.proto",
        "messaging.proto",
        "tachyon.proto",
        "tachyon_common.proto",
        "tachyon_enums.proto",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [
        "//frameworks/testing/rpcreplay/processors:rpc_replay_field_option_proto",
        "//google/api:annotations",
    ],
)

cc_proto_library(
    name = "metadata_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":metadata_proto"],
)

cc_proto_library(
    name = "tachyon_cc_proto",
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = ["//visibility:public"],
    deps = [":tachyon_proto"],
)
