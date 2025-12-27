cc_library(
    name = "types",
    srcs = [
        "device_info.cc",
    ],
    hdrs = [
        "atomics.h",
        "bluetooth_adapter.h",
        "generated/bluez_adapter_client_glue.h",
        "device_info.h",
        "multi_thread_executor.h",
        "scheduled_executor.h",
        "platform.h",
    ],
    visibility = ["//visibility:private"],
    deps = [
        "//internal/base:file_path",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings",
        "@sdbus_cpp//:sdbus_cpp",
        "@sdbus_cpp//:libsystemd",
    ],
)

cc_library(
    name = "linux",
    srcs = [
        "bluetooth_adapter.cc",
        "platform.cc",
        "scheduled_executor.cc",
    ],
    hdrs = [
        "device_info.h",
        "bluetooth_adapter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
            ":types",
            "//connections/implementation/flags:connections_flags",
            "//internal/base:file_path",
            "//internal/base:files",
            "//internal/flags:nearby_flags",
            "//internal/platform:base",
            "//internal/platform:cancellation_flag",
            "//internal/platform:logging",
            "//internal/platform:mac_address",
            "//internal/platform:types",
            "//internal/platform:uuid",
            "//internal/platform/flags:platform_flags",
            "//internal/platform/implementation:comm",
            "//internal/platform/implementation:platform",
            "//internal/platform/implementation:types",
            "//internal/platform/implementation:wifi_utils",
            "//internal/platform/implementation/shared:posix_condition_variable",
            "//internal/platform/implementation/shared:posix_mutex",
            "//internal/platform/implementation/shared:count_down_latch",
            "@com_google_absl//absl/base:core_headers",
            "@com_google_absl//absl/base:nullability",
            "@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/functional:any_invocable",
            "@com_google_absl//absl/memory",
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/status:statusor",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/synchronization",
            "@com_google_absl//absl/time",
            "@com_google_absl//absl/types:optional",
            "@com_google_absl//absl/types:span",
            "@nlohmann_json//:json",

            "@sdbus_cpp//:sdbus_cpp",
            "@sdbus_cpp//:libsystemd",
        ],
)

# The existing target that other BUILD files select on; keep for compatibility.
cc_library(
    name = "linux_platform_impl",
    srcs = [],
    hdrs = [],
    deps = [
        ":linux",
        ":types",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "multi_thread_executor_hdrs",
    hdrs = ["multi_thread_executor.h"],
    visibility = ["//visibility:public" ],
    deps = [
        "//internal/platform/implementation:types"
    ]
)

cc_test(
    name = "bluetooth_adapter_test",
    srcs = [
        "bluetooth_adapter_test.cc",
    ],
    deps = [
        ":linux",
        ":types",
        "//internal/platform/implementation:platform",
        "//internal/platform:base",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@sdbus_cpp//:sdbus_cpp",
        "@sdbus_cpp//:libsystemd",
    ],
)

cc_test(
    name = "multi_thread_executor_test",
    srcs = [
        "multi_thread_executor_test.cc",
    ],
    deps = [
        # Depend on the header-only target to pick up the linux header without
        # pulling system libraries.
        ":multi_thread_executor_hdrs",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",

    ],
)
