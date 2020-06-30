cc_library(
    name = "posix_mutex",
    srcs = [
        "posix_mutex.cc",
    ],
    hdrs = [
        "posix_mutex.h",
    ],
    visibility = [
        "//platform_v2/impl:__subpackages__",
    ],
    deps = ["//platform_v2/api:types"],
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
        "//platform_v2/impl:__subpackages__",
    ],
    deps = [
        ":posix_mutex",
        "//platform_v2/api:types",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    visibility = [
        "//platform_v2/impl:__subpackages__",
    ],
    deps = [
        "//platform_v2/api:types",
        "//platform_v2/base",
        "//absl/strings",
    ],
)

cc_test(
    name = "file_test",
    srcs = ["file_test.cc"],
    deps = [
        ":file",
        "//file/util:temp_path",
        "//platform_v2/base",
        "//testing/base/public:gunit_main",
        "//absl/strings",
    ],
)
