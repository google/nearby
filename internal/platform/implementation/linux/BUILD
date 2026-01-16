licenses(["notice"])

cc_library(
    name = "types",
    hdrs = [
        "atomic_boolean.h",
        "atomic_reference.h",
        "atomic_uint32.h",
        "condition_variable.h",
        "future.h",
        "mutex.h",
        "utils.h",
    ],
    copts = ["-lrt"],
    visibility = ["//visibility:public"],
    deps = [
        "//internal/platform/implementation:types",
        "@com_google_absl//absl/strings",
        "@sdbus_cpp//:libsystemd",
#        "@sdbus_cpp//:sdbus_cpp",
    ],
)


cc_library(
    name = "linux",
    srcs = [
        "platform.cc",
        "utils.cc",
    ],
    visibility = [
        "//internal/platform/implementation/linux:__subpackages__",
        "//connections:__subpackages__",
        "//fastpair:__subpackages__",
        "//location/nearby:__subpackages__",
        "//presence:__subpackages__",
        "//third_party/nearby/sharing:__subpackages__",
    ],
    deps = [
        ":types",
        "//internal/flags:nearby_flags",
        "//internal/platform:base",
        "//internal/platform:cancellation_flag",
        "//internal/platform:comm",
        "//internal/platform:types",
        "//internal/platform:uuid",
        "//internal/platform/flags:platform_flags",
        "//internal/platform/implementation:comm",
        "//internal/platform/implementation:platform",
        "//internal/platform/implementation:types",
        "//internal/platform/implementation/shared:count_down_latch",
        "//internal/platform/implementation/shared:file",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@nlohmann_json//:json",
        "@sdbus_cpp//:libsystemd",
#        "@sdbus_cpp//:sdbus_cpp",
    ],
)

