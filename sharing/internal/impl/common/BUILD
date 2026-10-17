load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//sharing:__subpackages__"])

licenses(["notice"])

cc_library(
    name = "preference_manager",
    srcs = ["preference_manager.cc"],
    hdrs = ["preference_manager.h"],
    deps = [
        "//internal/platform/implementation:types",
        "//location/nearby/sharing/lib/sync:sync_binding_prefs_cc_proto",
        "//location/nearby/sharing/lib/sync:sync_config_prefs_cc_proto",
        "//sharing/internal/api:platform",
        "//sharing/internal/public:pref_names",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "public_certificate_database",
    srcs = ["public_certificate_database.cc"],
    hdrs = ["public_certificate_database.h"],
    deps = [
        "//sharing/internal/api:platform",
        "//sharing/proto:share_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "offline_account_manager",
    hdrs = ["offline_account_manager.h"],
    deps = [
        "//location/nearby/sharing/lib/account:account_manager",
        "//location/nearby/sharing/lib/account:signin_attempt",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
    ],
)
