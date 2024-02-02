licenses(["notice"])

cc_library(
    name = "contacts",
    srcs = [
        "nearby_share_contact_manager.cc",
        "nearby_share_contact_manager_impl.cc",
        "nearby_share_contacts_sorter.cc",
    ],
    hdrs = [
        "nearby_share_contact_manager.h",
        "nearby_share_contact_manager_impl.h",
        "nearby_share_contacts_sorter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/base",
        "//internal/crypto_cros",
        "//internal/platform:types",
        "//internal/platform/implementation:types",
        "//sharing/common",
        "//sharing/internal/api:platform",
        "//sharing/internal/base",
        "//sharing/internal/public:logging",
        "//sharing/internal/public:types",
        "//sharing/local_device_data",
        "//sharing/proto:share_cc_proto",
        "//sharing/scheduling",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_library(
    name = "test_support",
    testonly = True,
    srcs = [
        "fake_nearby_share_contact_manager.cc",
    ],
    hdrs = [
        "fake_nearby_share_contact_manager.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":contacts",
        "//internal/platform/implementation:types",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:types",
        "//sharing/local_device_data",
    ],
)

cc_test(
    name = "contacts_test",
    srcs = [
        "nearby_share_contact_manager_impl_test.cc",
        "nearby_share_contacts_sorter_test.cc",
    ],
    deps = [
        ":contacts",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "//internal/test",
        "//sharing/common",
        "//sharing/internal/api:mock_sharing_platform",
        "//sharing/internal/api:platform",
        "//sharing/internal/test:nearby_test",
        "//sharing/local_device_data:test_support",
        "//sharing/proto:share_cc_proto",
        "//sharing/scheduling",
        "//sharing/scheduling:test_support",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)