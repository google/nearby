licenses(["notice"])

cc_library(
	name = "types",
    hdrs = [
        "device_info.h",
    ],
	srcs = [
	    "device_info.cc",
        "bluetooth_adapter.h",
	],
    visibility = ["//third_party/nearby/sharing/internal/impl/linux:__pkg__"],
    deps = [
        ":comm",
        "//internal/platform/implementation:types",
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@libsystemd//:lib",
        "@sdbus_cpp//:lib",
    ],
)

cc_library(
    name = "comm",
    hdrs = [
        "bluetooth_adapter.h",
        "bluetooth_bluez_profile.h",
        "bluetooth_classic_device.h",
        "bluetooth_classic_medium.h",
        "bluetooth_classic_server_socket.h",
        "bluetooth_classic_socket.h",
        "bluetooth_pairing.h",
        "bluez.h",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@libsystemd//:lib",
    ],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "linux",
    srcs = [
        "bluetooth_adapter.cc",
        "bluetooth_bluez_profile.cc",
        "bluetooth_classic_device.cc",
        "bluetooth_classic_medium.cc",
        "bluetooth_classic_socket.cc",
        "bluetooth_pairing.cc",
    ],
    visibility = [
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//location/nearby:__subpackages__",
        "//presence:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        ":comm",
        "//internal/platform/implementation:types",
        "//internal/platform:logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@libsystemd//:lib",
    ]
)
