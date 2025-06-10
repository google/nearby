# third_party/nearby/connections/c/core/BUILD

package(default_visibility = [
    "//connections/c:__subpackages__",
    "//connections/dart:__subpackages__",
])

# C++ interface for shared buffer manager
cc_library(
    name = "shared_buffer_manager",
    hdrs = ["shared_buffer_manager.h"],
    compatible_with = ["//buildenv/target:non_prod"],
    deps = [],
)

cc_library(
    name = "shared_buffer_stream",
    srcs = [
        "shared_buffer_stream.cc",
    ],
    hdrs = [
        "shared_buffer_manager.h",
        "shared_buffer_stream.h",
    ],
    compatible_with = ["//buildenv/target:non_prod"],
    linkopts = select({
        "@platforms//os:apple": [
            "-framework Foundation",
            "-framework CoreFoundation",
        ],
        "//conditions:default": [],
    }),
    deps = [
        "//internal/platform:base",
        "//internal/platform:logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
    ] + select({
        "@platforms//os:apple": [
            "//third_party/apple_frameworks:CoreFoundation",
            "//third_party/apple_frameworks:Foundation",
        ],
        "//conditions:default": [
            # Add other dependencies if needed for other platforms
        ],
    }),
    alwayslink = 1,
)
