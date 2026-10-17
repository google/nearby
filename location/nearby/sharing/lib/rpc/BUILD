load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Stand-ins for the Google-internal Nearby identity RPC client.
cc_library(
    name = "sharing_rpc_client",
    hdrs = ["sharing_rpc_client.h"],
    deps = [
        "//google/nearby/identity/v1:rpcs_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "grpc_async_client_factory",
    srcs = ["grpc_async_client_factory.cc"],
    hdrs = ["grpc_async_client_factory.h"],
    deps = [
        ":sharing_rpc_client",
        "//google/nearby/identity/v1:rpcs_cc_proto",
        "//internal/platform:types",
        "//location/nearby/sharing/lib/account:account_manager",
        "//sharing/analytics",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)
