load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "common",
    srcs = ["nearby_sharing_common.cc"],
    hdrs = ["nearby_sharing_common.h"],
    deps = ["//proto:sharing_enums_cc_proto"],
)
