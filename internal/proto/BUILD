proto_library(
    name = "credential_proto",
    srcs = ["credential.proto"],
)

cc_proto_library(
    name = "credential_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":credential_proto"],
)

java_lite_proto_library(
    name = "credential_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/presence:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/presence:__subpackages__",
    ],
    deps = [
        ":credential_proto",
    ],
)

proto_library(
    name = "local_credential_proto",
    srcs = ["local_credential.proto"],
    deps = [":credential_proto"],
)

cc_proto_library(
    name = "local_credential_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":local_credential_proto"],
)

java_lite_proto_library(
    name = "local_credential_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/presence:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/presence:__subpackages__",
    ],
    deps = [
        ":local_credential_proto",
    ],
)

proto_library(
    name = "metadata_proto",
    srcs = ["metadata.proto"],
)

cc_proto_library(
    name = "metadata_cc_proto",
    visibility = [
        "//:__subpackages__",
    ],
    deps = [":metadata_proto"],
)

java_lite_proto_library(
    name = "metadata_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/presence:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby/src/com/google/android/gms/nearby/presence:__subpackages__",
    ],
    deps = [
        ":metadata_proto",
    ],
)
