package(default_applicable_licenses = [
    "//third_party/nearby:license",  # copybara:strip
])

licenses(["notice"])

cc_library(
    name = "nearby_fastpair_test",
    hdrs = [
        "fast_pair_fake_http_client.h",
    ],
    copts = [
        "-Ithird_party",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//internal/network:types",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_test(
    name = "nearby_fastpair_test_test",
    size = "small",
    timeout = "short",
    srcs = [
        "fast_pair_fake_http_client_test.cc",
    ],
    copts = [
        "-Ithird_party",
    ],
    deps = [
        ":nearby_fastpair_test",
        "//internal/network:types",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
