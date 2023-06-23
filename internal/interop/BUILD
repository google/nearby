cc_library(
    name = "authentication_transport_interface",
    hdrs = [
        "authentication_transport.h",
    ],
    visibility = [
        "//:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "device",
    hdrs = [
        "device.h",
        "device_provider.h",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//presence:__subpackages__",
    ],
    deps = [
        ":authentication_transport_interface",
        "//internal/platform:connection_info",
        "//internal/platform:types",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
    ],
)
