cc_library(
    name = "core",
    srcs = [
        "core.cc",
    ],
    hdrs = [
        "core.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
    ],
    deps = [
        ":core_types",
        "//core/internal",
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
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
        "//core:__subpackages__",
    ],
    deps = [
        "//platform/base",
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
        "//proto:connections_enums_portable_proto",
        "//absl/strings",
        "//absl/types:variant",
    ],
)

cc_test(
    name = "core_test",
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
        ":core",
        ":core_types",
        "//core/internal",
        "//core/internal:internal_test",
        "//platform/base",
        "//platform/impl/g3",  # build_cleaner: keep
        "//platform/public:comm",
        "//platform/public:logging",
        "//platform/public:types",
        "//testing/base/public:gunit_main",
        "//absl/strings",
        "//absl/time",
        "//absl/types:variant",
    ],
)
