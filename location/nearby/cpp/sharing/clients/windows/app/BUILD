load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

licenses(["notice"])

# Quick Share for Windows desktop (Win32 UI over the Nearby Sharing service).
cc_binary(
    name = "quick_share",
    srcs = [
        "app_controller.cc",
        "app_controller.h",
        "log_file.cc",
        "log_file.h",
        "main.cc",
        "main_window.cc",
        "main_window.h",
        "tray_icon.cc",
        "tray_icon.h",
        "wifi_join.cc",
        "wifi_join.h",
    ],
    defines = [
        "UNICODE",
        "_UNICODE",
    ],
    linkopts = [
        "/SUBSYSTEM:WINDOWS",
        "/ENTRY:wWinMainCRTStartup",
        # main.cc adds the Common Controls 6 manifest dependency.
        "/MANIFEST:EMBED",
        "comctl32.lib",
        "gdi32.lib",
        "ole32.lib",
        "shell32.lib",
        "user32.lib",
        "advapi32.lib",
        "wlanapi.lib",
    ],
    deps = [
        "//internal/base:file_path",
        "//internal/platform:logging",
        "//location/nearby/sharing/lib/analytics",
        "//sharing:attachments",
        "//sharing:nearby_sharing_service",
        "//sharing:transfer_metadata",
        "//sharing:types",
        "//sharing/internal/impl/windows:platform_windows_libs",
        "//sharing/proto:enums_cc_proto",
        "//third_party/WinRT:winrt",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/log:log_entry",
        "@com_google_absl//absl/log:log_sink",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)
