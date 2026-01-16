load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_test(
    name = "linux_test",
    size = "small",
    srcs = [
        "atomic_boolean_test.cc",
        "atomic_reference_test.cc",
        "count_down_latch_test.cc",
        "platform_test.cc",
        "utils_test.cc",
    ],
    deps = [
        "//internal/base:file_path",
        "//internal/base:files",
        "//internal/platform:logging",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation/linux:linux",
        "//internal/platform/implementation/linux:types",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)


