licenses(["notice"])

cc_library(
    name = "test",
    srcs = [
        "fake_clock.cc",
        "fake_task_runner.cc",
        "fake_timer.cc",
    ],
    hdrs = [
        "fake_clock.h",
        "fake_device_info.h",
        "fake_task_runner.h",
        "fake_timer.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/base:bluetooth_address",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",  # fixdeps: keep
    ],
)

cc_test(
    name = "test_test",
    size = "small",
    timeout = "short",
    srcs = [
        "fake_clock_test.cc",
        "fake_device_info_test.cc",
        "fake_task_runner_test.cc",
        "fake_timer_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    shard_count = 8,
    deps = [
        ":test",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)