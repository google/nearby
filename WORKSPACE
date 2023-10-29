local_repository(
    name = "bazel_skylib",
    path = "third_party/bazel_skylib",
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

# Rule repository, note that it's recommended to use a pinned commit to a released version of the rules
local_repository(
    name = "rules_foreign_cc",
    path = "third_party/rules_foreign_cc",
    # tag: 0.6.0
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

# This sets up some common toolchains for building targets. For more details, please see
# https://github.com/bazelbuild/rules_foreign_cc/tree/main/docs#rules_foreign_cc_dependencies
rules_foreign_cc_dependencies()

local_repository(
    name = "com_google_absl",
    path = "third_party/absl",
)

local_repository(
    name = "com_google_protobuf",
    path = "third_party/protobuf",
)

local_repository(
    name = "com_google_glog",
    path = "third_party/glog",
)

local_repository(
    name = "com_google_ukey2",
    path = "third_party/ukey2",
)

local_repository(
    name = "aappleby_smhasher",
    path = "third_party/smhasher",
)

local_repository(
    name = "nlohmann_json",
    path = "third_party/json",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

local_repository(
    name = "dart_lang",
    path = "third_party/dart_lang",
)

# Load common dependencies.
protobuf_deps()

local_repository(
    name = "com_google_googletest",
    path = "third_party/gtest",
)

# gflags needed by glog
local_repository(
    name = "com_github_gflags_gflags",
    path = "third_party/gflags",
)

# ----------------------------------------------
# Nisaba: Script processing library from Google:
# ----------------------------------------------
# We depend on some of core C++ libraries from Nisaba and use the fresh code
# from the HEAD. See
#   https://github.com/google-research/nisaba

# nisaba_version = "main"

# http_archive(
#     name = "com_google_nisaba",
#     strip_prefix = "nisaba-%s" % nisaba_version,
#     url = "https://github.com/google-research/nisaba/archive/refs/heads/%s.zip" % nisaba_version,
# )

# load("@com_google_nisaba//bazel:workspace.bzl", "nisaba_public_repositories")

# nisaba_public_repositories()

local_repository(
    name = "boringssl",
    path = "third_party/boringssl",
)

# -------------------------------------------------------------------------
# Protocol buffer matches (should be part of gmock and gtest, but not yet):
#   https://github.com/inazarenko/protobuf-matchers
local_repository(
    name = "com_github_protobuf_matchers",
    path = "third_party/protobuf_matchers",
)

local_repository(
    name = "com_googlesource_code_re2",
    path = "third_party/re2",
)
