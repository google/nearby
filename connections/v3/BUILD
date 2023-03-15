cc_library(
    name = "v3_types",
    hdrs = [
        "bandwidth_info.h",
        "connection_listening_options.h",
        "connection_resolution.h",
        "listeners.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//internal:device",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)
