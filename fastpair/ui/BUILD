licenses(["notice"])

cc_library(
    name = "fast_pair_ui",
    srcs = [
        "fast_pair/fast_pair_notification_controller.cc",
        "fast_pair/fast_pair_presenter_impl.cc",
        "ui_broker_impl.cc",
    ],
    hdrs = [
        "actions.h",
        "fast_pair/fast_pair_notification_controller.h",
        "fast_pair/fast_pair_presenter.h",
        "fast_pair/fast_pair_presenter_impl.h",
        "ui_broker.h",
        "ui_broker_impl.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "//fastpair/common",
        "//fastpair/repository",
        "//fastpair/server_access",
        "//internal/base",
        "//internal/platform:logging",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "fake_fast_pair_ui",
    hdrs = [
        "fast_pair/fake_fast_pair_notification_controller_observer.h",
        "fast_pair/fake_fast_pair_presenter.h",
    ],
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        ":fast_pair_ui",
        "//fastpair/repository",
    ],
)

cc_test(
    name = "fast_pair_ui_test",
    srcs = [
        "fast_pair/fast_pair_notification_controller_test.cc",
        "fast_pair/fast_pair_presenter_impl_test.cc",
        "ui_broker_impl_test.cc",
    ],
    deps = [
        ":fake_fast_pair_ui",
        ":fast_pair_ui",
        "//fastpair/common",
        "//fastpair/proto:fastpair_cc_proto",
        "//fastpair/repository",
        "//fastpair/server_access:test_support",
        "//internal/network:types",
        "//internal/platform/implementation/g3",
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
