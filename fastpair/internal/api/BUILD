licenses(["notice"])

cc_library(
    name = "platform",
    compatible_with = ["//buildenv/target:non_prod"],
    textual_hdrs = glob(["**/*.h"]),
    visibility = [
        "//fastpair:__subpackages__",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)
