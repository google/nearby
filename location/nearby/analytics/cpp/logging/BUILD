load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "event_logger",
    hdrs = ["event_logger.h"],
    deps = [
        "//location/nearby/analytics/cpp/proto:connections_log_cc_proto",
        "//location/nearby/analytics/cpp/proto:sharing_log_cc_proto",
    ],
)
