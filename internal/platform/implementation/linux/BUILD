licenses(["notice"])

cc_library(
	name = "types",
    hdrs = [
        "device_info.h",
    ],
	srcs = [
	    "device_info.cc",
	],
    deps = [
        "//internal/platform/implementation:types",
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@libsystemd//:lib",
    ],
)

cc_library(
    name = "linux",
    hdrs = [
        "bluetooth_adapter.h",
        "bluez.h"
    ],
    srcs = ["bluetooth_adapter.cc"],
    deps = [
        "//internal/platform/implementation:types",
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@libsystemd//:lib",
    ]
)
