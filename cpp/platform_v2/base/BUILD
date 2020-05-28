load("//ads/util/non_compile:non_compile.bzl", "cc_with_non_compile_test")

cc_library(
    name = "base",
    srcs = [
        "base64_utils.cc",
        "prng.cc",
    ],
    hdrs = [
        "base64_utils.h",
        "byte_array.h",
        "callable.h",
        "exception.h",
        "input_stream.h",
        "listeners.h",
        "output_stream.h",
        "prng.h",
        "runnable.h",
        "socket.h",
    ],
    visibility = [
        "//core_v2:__subpackages__",
        "//platform_v2:__subpackages__",
        "//platform_v2/api:__subpackages__",
    ],
    deps = [
        "//absl/strings",
        "//absl/time",
    ],
)

cc_library(
    name = "util",
    srcs = [
        "base_pipe.cc",
    ],
    hdrs = [
        "base_mutex_lock.h",
        "base_pipe.h",
    ],
    visibility = [
        "//platform_v2/impl:__subpackages__",
        "//platform_v2/public:__pkg__",
    ],
    deps = [
        ":base",
        "//platform_v2/api",
        "//absl/base:core_headers",
    ],
)

cc_test(
    name = "platform_base_test",
    srcs = [
        "byte_array_test.cc",
        "prng_test.cc",
    ],
    deps = [
        ":base",
        "//testing/base/public:gunit_main",
    ],
)

cc_with_non_compile_test(
    name = "exception_test",
    srcs = [
        "exception_test.cc",
    ],
    deps = [
        ":base",
        "//testing/base/public:gunit_main",
    ],
)
