cc_library(
    name = "v0",
    hdrs = [
        "client_socket.h",
        "server_socket.h",
    ],
    visibility = [
        "//internal:__subpackages__",
    ],
    deps = [
        "//internal/socket_abstraction",
    ],
)

cc_test(
    name = "v0_test",
    srcs = [
        "client_server_functional_test.cc",
    ],
    deps = [
        ":v0",
        "//internal/platform:base",
        "//internal/platform:logging",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "//internal/socket_abstraction",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
