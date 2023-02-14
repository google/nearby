cc_library(
    name = "v1",
    srcs = [
        "client_socket.cc",
        "server_socket.cc",
    ],
    hdrs = [
        "client_socket.h",
        "initial_data_provider.h",
        "server_socket.h",
    ],
    visibility = [
        "//internal:__subpackages__",
    ],
    deps = [
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/socket_abstraction",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "v1_test",
    srcs = [
        "client_socket_test.cc",
        "server_socket_test.cc",
    ],
    deps = [
        ":v1",
        "//internal/platform:base",
        "//internal/platform:types",
        "//internal/platform/implementation/g3",  # build_cleaner: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
