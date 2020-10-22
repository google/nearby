cc_library(
    name = "posix_mutex",
    srcs = [
        "posix_mutex.cc",
    ],
    hdrs = [
        "posix_mutex.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = ["//platform/api:types"],
)

cc_library(
    name = "posix_condition_variable",
    srcs = [
        "posix_condition_variable.cc",
    ],
    hdrs = [
        "posix_condition_variable.h",
    ],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
    ],
    deps = [
        ":posix_mutex",
        "//platform/api:types",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    visibility = [
        "//googlemac/iPhone/Shared/Nearby/Connections:__subpackages__",
        "//platform/impl:__subpackages__",
    ],
    deps = [
        "//platform/api:types",
        "//platform/base",
        "//absl/strings",
    ],
)

cc_test(
    name = "file_test",
    srcs = ["file_test.cc"],
    deps = [
        ":file",
        "//file/util:temp_path",
        "//platform/base",
        "//testing/base/public:gunit_main",
        "//absl/strings",
    ],
)
