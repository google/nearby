load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Rule repository, note that it's recommended to use a pinned commit to a released version of the rules
http_archive(
    name = "rules_foreign_cc",
    strip_prefix = "rules_foreign_cc-0.6.0",
    url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.6.0.tar.gz",
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

# This sets up some common toolchains for building targets. For more details, please see
# https://github.com/bazelbuild/rules_foreign_cc/tree/main/docs#rules_foreign_cc_dependencies
rules_foreign_cc_dependencies()

_ALL_CONTENT = """\\\r
filegroup(\r
    name = "all_srcs",\r
    srcs = glob(["**"]),\r
    visibility = ["//visibility:public"],\r
)\r
"""

local_repository(
    name = "com_google_absl",
    path = "third_party/absl",
)

# Using a protobuf javalite version that contains @com_google_protobuf_javalite//:javalite_toolchain
# http_archive(
#     name = "com_google_protobuf_javalite",
#     strip_prefix = "protobuf-javalite",
#     urls = ["https://github.com/google/protobuf/archive/javalite.zip"],
# )

local_repository(
    name = "com_google_protobuf",
    path = "third_party/protobuf",
)

# http_archive(
#     name = "com_google_protobuf_cc",
#     strip_prefix = "protobuf-3.17.0",
#     urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.17.0.tar.gz"],
# )

# http_archive(
#     name = "com_google_protobuf_java",
#     strip_prefix = "protobuf-3.17.0",
#     urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.17.0.tar.gz"],
# )

local_repository(
    name = "com_google_glog",
    path = "third_party/glog",
)

local_repository(
    name = "com_google_ukey2",
    path = "third_party/ukey2",
)

local_repository(
    name = "smhasher",
    path = "third_party/smasher",
)

# http_archive(
#     name = "aappleby_smhasher",
#     build_file_content = """
# package(default_visibility = ["//visibility:public"])
# cc_library(
#     name = "libmurmur3",
#     srcs = ["src/MurmurHash3.cpp"],
#     hdrs = ["src/MurmurHash3.h"],
#     copts = ["-Wno-implicit-fallthrough"],
#     licenses = ["unencumbered"],  # MurmurHash is explicity public-domain
# )""",
#     strip_prefix = "smhasher-master",
#     urls = ["https://github.com/aappleby/smhasher/archive/master.zip"],
# )

local_repository(
    name = "json",
    path = "third_party/json",
)
# http_archive(
#     name = "nlohmann_json",
#     build_file_content = """
# cc_library(
#   name = "json",
#   hdrs = glob([
#     "include/nlohmann/**/*.hpp",
#   ]),
#   includes = ["include"],
#   visibility = ["//visibility:public"],
#   alwayslink = True,
# )""",
#     strip_prefix = "json-3.10.5",
#     urls = [
#         "https://github.com/nlohmann/json/archive/refs/tags/v3.10.5.tar.gz",
#     ],
# )

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

# Load common dependencies.
protobuf_deps()

local_repository(
    name = "com_google_googletest",
    path = "third_party/gtest",
)

# http_archive(
#     name = "com_google_webrtc",
#     build_file_content = """
# package(default_visibility = ["//visibility:public"])
# """,
#     urls = ["https://webrtc.googlesource.com/src/+archive/main.tar.gz"],
# )

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

# http_archive(
#     name = "boringssl",
#     sha256 = "5d299325d1db8b2f2db3d927c7bc1f9fcbd05a3f9b5c8239fa527c09bf97f995",  # Last updated 2022-10-19
#     strip_prefix = "boringssl-0acfcff4be10514aacb98eb8ab27bb60136d131b",
#     urls = ["https://github.com/google/boringssl/archive/0acfcff4be10514aacb98eb8ab27bb60136d131b.tar.gz"],
# )

# -------------------------------------------------------------------------
# Protocol buffer matches (should be part of gmock and gtest, but not yet):
#   https://github.com/inazarenko/protobuf-matchers

local_repository(
    name = "com_github_protobuf_matchers",
    path = "third_party/protobuf_matchers",
)

# http_archive(
#     name = "com_googlesource_code_re2",
#     sha256 = "26155e050b10b5969e986dab35654247a3b1b295e0532880b5a9c13c0a700ceb",
#     strip_prefix = "re2-2021-06-01",
#     urls = [
#         "https://github.com/google/re2/archive/refs/tags/2021-06-01.tar.gz",
#     ],
# )
