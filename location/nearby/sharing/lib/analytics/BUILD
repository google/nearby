load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "analytics",
    hdrs = ["analytics_recorder_impl.h"],
    deps = [
        "//location/nearby/analytics/cpp/logging:event_logger",
        "//proto:sharing_enums_cc_proto",
        "//sharing:attachments",
        "//sharing:types",
        "//sharing/analytics",
        "//sharing/common:enum",
        "//sharing/proto:enums_cc_proto",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)
