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
        "device_info.cc",
        "fast_pair_platform.cc",
        "timer.cc",
        "utils.cc",
    ],
    hdrs = [
        "ble_gatt_client.h",
        "device_info.h",
        "timer.h",
        "utils.h",
    ],
    copts = [
        "-Ithird_party/nearby/internal/platform/implementation/windows/generated",
    ],
    defines = ["_SILENCE_CLANG_COROUTINE_MESSAGE"],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/internal/api:platform",
        "//fastpair/internal/base:bluetooth_address",
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

cc_test(
    name = "platform_windows_test",
    size = "small",
    timeout = "short",
    srcs = [
        "device_info_test.cc",
        "timer_test.cc",
    ],
    copts = [
        "-Ithird_party/nearby/internal/platform/implementation/windows/generated",
    ],
    shard_count = 1,
    deps = [
        ":platform_windows",
        ":platform_windows_libs",
        "//fastpair/internal/api:platform",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation/windows",
        "//internal/platform/implementation/windows/generated:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)
