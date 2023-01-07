licenses(["notice"])

cc_library(
    name = "types",
    hdrs = [
        "device_info.h",
        "task_runner.h",
        "timer.h",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        "//fastpair/internal/api:platform",
        "//fastpair/internal/impl/g3:platform_g3",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "fastpair_context",
    srcs = [
        "task_runner_impl.cc",
        "timer_impl.cc",
    ],
    hdrs = [
        "task_runner_impl.h",
        "timer_impl.h",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        ":device_info",
        ":types",
        "//fastpair/internal/api:platform",
        "//fastpair/internal/impl/g3:platform_g3",
        "//internal/platform:logging",
        "//internal/platform:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "device_info",
    srcs = [
        "device_info_impl.cc",
    ],
    hdrs = [
        "device_info_impl.h",
    ],
    visibility = ["//fastpair:__subpackages__"],
    deps = [
        ":types",
        "//fastpair/internal/api:platform",
        "//fastpair/internal/impl/g3:platform_g3",
    ],
)

cc_test(
    name = "fastpair_context_test",
    size = "small",
    timeout = "short",
    srcs = [
        "task_runner_impl_test.cc",
        "timer_impl_test.cc",
    ],
    shard_count = 8,
    deps = [
        "device_info",
        ":fastpair_context",
        ":types",
        "//fastpair/internal/api:platform",
        "//fastpair/internal/impl/g3:platform_g3",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
