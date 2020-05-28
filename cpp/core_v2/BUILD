cc_library(
    name = "core_v2",
    srcs = [
        "core.cc",
    ],
    hdrs = [
        "core.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
    ],
    deps = [
        ":core_types",
        "//core_v2/internal",
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//absl/strings",
        "//absl/time",
        "//absl/types:span",
    ],
)

cc_library(
    name = "core_types",
    srcs = [
        "strategy.cc",
    ],
    hdrs = [
        "listeners.h",
        "options.h",
        "params.h",
        "payload.h",
        "status.h",
        "strategy.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
    ],
    deps = [
        "//platform_v2/base",
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//absl/strings",
        "//absl/types:variant",
    ],
)

cc_test(
    name = "core_v2_test",
    size = "small",
    srcs = [
        "core_test.cc",
        "listeners_test.cc",
        "payload_test.cc",
        "status_test.cc",
        "strategy_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":core_types",
        ":core_v2",
        "//core_v2/internal",
        "//core_v2/internal:internal_test",
        "//platform_v2/base",
        "//platform_v2/impl/g3",
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/time",
        "//absl/types:variant",
    ],
)
