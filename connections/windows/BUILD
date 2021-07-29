load("//third_party/msvc:build_windows.bzl", "cc_windows_dll")

# Build with --config=lexan
cc_windows_dll(
    name = "core_adapter",
    srcs = ["core_adapter.cc"],
    hdrs = ["core_adapter.h"],
    copts = ["-DCORE_ADAPTER_BUILD_DLL"],
    defines = ["CORE_ADAPTER_DLL"],
    tags = ["windows-dll"],
    deps = [
        "//absl/strings",
        "//core",
        "//platform/impl/windows",
        "//webrtc/api:create_peerconnection_factory",
    ],
)
