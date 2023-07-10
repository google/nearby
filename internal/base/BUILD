licenses(["notice"])

cc_library(
    name = "base",
    srcs = [
    ],
    hdrs = [
        "observer_list.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = [
        "//fastpair:__subpackages__",
        "//internal/account:__subpackages__",
        "//internal/interop:__pkg__",
        "//internal/platform:__pkg__",
        "//location/nearby/cpp/experiments:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        "//internal/platform:types",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

cc_library(
    name = "bluetooth_address",
    srcs = [
        "bluetooth_address.cc",
    ],
    hdrs = [
        "bluetooth_address.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = [
        "//fastpair:__subpackages__",
        "//internal:__subpackages__",
        "//location/nearby/cpp/sharing:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "base_test",
    size = "small",
    timeout = "short",
    srcs = [
        "bluetooth_address_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    shard_count = 8,
    deps = [
        ":bluetooth_address",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
