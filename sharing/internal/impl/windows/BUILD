load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Windows implementation of the Nearby Sharing platform (SharingPlatform).
cc_library(
    name = "platform_windows_libs",
    srcs = [
        "app_info.cc",
        "bluetooth_adapter.cc",
        "fast_initiation_manager.cc",
        "network_monitor.cc",
        "system_info.cc",
        "windows_platform.cc",
    ],
    hdrs = [
        "app_info.h",
        "bluetooth_adapter.h",
        "fast_initiation_manager.h",
        "network_monitor.h",
        "system_info.h",
        "windows_platform.h",
    ],
    defines = [
        "_WIN32_WINNT=_WIN32_WINNT_WIN10",
        "WINVER=_WIN32_WINNT_WIN10",
    ],
    linkopts = [
        "advapi32.lib",
        "setupapi.lib",
    ],
    tags = ["windows"],
    deps = [
        "//internal/base:file_path",
        "//internal/platform:logging",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/windows",
        "//internal/platform/implementation/windows:device_paths",
        "//location/nearby/sharing/lib/account:account_manager",
        "//sharing/internal/impl/common:offline_account_manager",
        "//sharing/internal/impl/common:preference_manager",
        "//sharing/internal/impl/common:public_certificate_database",
        "//internal/platform:mac_address",
        "//internal/platform/implementation/windows:string_utils",
        "//sharing/internal/api:platform",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "fast_initiation_manager_test",
    srcs = ["fast_initiation_manager_test.cc"],
    tags = ["windows"],
    deps = [
        ":platform_windows_libs",
        "@com_google_googletest//:gtest_main",
    ],
)
