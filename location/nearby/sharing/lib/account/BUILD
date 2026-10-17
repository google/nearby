load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Stand-in for the Google-internal account manager interface.
cc_library(
    name = "account_manager",
    hdrs = ["account_manager.h"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "signin_attempt",
    hdrs = ["signin_attempt.h"],
)
