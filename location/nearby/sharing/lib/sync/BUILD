load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Stand-ins for Google-internal sync prefs protos, recovered from the
# descriptors embedded in Quick Share for Windows (see file headers).
proto_library(
    name = "sync_binding_prefs_proto",
    srcs = ["sync_binding_prefs.proto"],
)

cc_proto_library(
    name = "sync_binding_prefs_cc_proto",
    deps = [":sync_binding_prefs_proto"],
)

proto_library(
    name = "sync_config_prefs_proto",
    srcs = ["sync_config_prefs.proto"],
    deps = ["@com_google_protobuf//:timestamp_proto"],
)

cc_proto_library(
    name = "sync_config_prefs_cc_proto",
    deps = [":sync_config_prefs_proto"],
)

# Stand-in for the Google-internal file-sync binding manager.
cc_library(
    name = "sync_manager",
    srcs = ["sync_manager.cc"],
    hdrs = ["sync_manager.h"],
    deps = [
        ":sync_binding_prefs_cc_proto",
        ":sync_config_prefs_cc_proto",
        "//internal/base:file_path",
        "//location/nearby/sharing/lib/rpc:sharing_rpc_client",
        "//sharing/internal/api:platform",
        "//third_party/gloop/util/time:protoutil",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)
