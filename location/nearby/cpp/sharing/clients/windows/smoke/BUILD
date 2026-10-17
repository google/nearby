load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

licenses(["notice"])

cc_binary(
    name = "quick_share_smoke",
    srcs = ["main.cc"],
    deps = [
        "//location/nearby/sharing/lib/analytics",
        "//sharing:nearby_sharing_service",
        "//sharing:attachments",
        "//sharing:transfer_metadata",
        "//sharing:types",
        "//sharing/internal/impl/windows:platform_windows_libs",
        "//sharing/proto:enums_cc_proto",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)
