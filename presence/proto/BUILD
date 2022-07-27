proto_library(
    name = "device_metadata_proto",
    srcs = ["device_metadata.proto"],
)

cc_proto_library(
    name = "device_metadata_cc_proto",
    visibility = [
        "//third_party/nearby:__subpackages__",
    ],
    deps = [":device_metadata_proto"],
)
