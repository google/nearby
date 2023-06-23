cc_library(
    name = "v3_types",
    hdrs = [
        "bandwidth_info.h",
        "connection_listening_options.h",
        "connection_result.h",
        "connections_device.h",
        "connections_device_provider.h",
        "listeners.h",
        "listening_result.h",
        "params.h",
    ],
    visibility = [
        "//connections:__subpackages__",
    ],
    deps = [
        "//connections:core_types",
        "//internal/crypto",
        "//internal/interop:device",
        "//internal/platform:connection_info",
        "//proto:connections_enums_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_test(
    name = "v3_types_test",
    srcs = [
        "connections_device_provider_test.cc",
        "connections_device_test.cc",
    ],
    deps = [
        "//connections/implementation:internal_test",
        "//connections/v3:v3_types",
        "//internal/platform:connection_info",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
