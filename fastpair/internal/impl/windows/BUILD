licenses(["notice"])

cc_library(
    name = "platform_windows_libs",
    linkopts = [
        "wininet.lib",
        "kernel32.lib",
        "user32.lib",
        "gdi32.lib",
        "winspool.lib",
        "comdlg32.lib",
        "advapi32.lib",
        "shell32.lib",
        "ole32.lib",
        "oleaut32.lib",
        "uuid.lib",
        "odbc32.lib",
        "odbccp32.lib",
        "wbemuuid.lib",
        "wtsapi32.lib",
        "version.lib",
    ],
    visibility = ["//:__subpackages__"],
)

cc_library(
    name = "platform_windows",
    srcs = [
        "ble_gatt_client.cc",
        "utils.cc",
    ],
    hdrs = [
        "ble_gatt_client.h",
        "utils.h",
    ],
    copts = [
        "-Ithird_party/nearby/internal/platform/implementation/windows/generated",
    ],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation/windows",
        "//internal/platform/implementation/windows/generated:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)
