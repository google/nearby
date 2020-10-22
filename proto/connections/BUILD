load("//net/proto2/contrib/portable/cc:portable_proto_build_defs.bzl", "portable_proto_library")

proto_library(
    name = "offline_wire_formats_proto",
    srcs = [
        "offline_wire_formats.proto",
    ],
    cc_api_version = 2,
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
    ],
)

java_lite_proto_library(
    name = "offline_wire_formats_java_proto_lite",
    visibility = [
        "//java/com/google/android/gmscore/integ/modules/nearby:__subpackages__",
        "//javatests/com/google/android/gmscore/integ/modules/nearby:__subpackages__",
    ],
    deps = [":offline_wire_formats_proto"],
)

portable_proto_library(
    name = "offline_wire_formats_portable_proto",
    config = ":offline_wire_formats_proto_config",
    copts = [
        "-DGOOGLE_PROTOBUF_NO_RTTI=1",
    ],
    header_outs = [
        "offline_wire_formats.pb.h",
    ],
    proto_deps = [
        ":offline_wire_formats_proto",
    ],
    visibility = [
        "//location/nearby/connections:__subpackages__",
        "//platform:__subpackages__",
    ],
)

filegroup(
    name = "offline_wire_formats_proto_config",
    srcs = ["offline_wire_formats_proto_config.asciipb"],
)
