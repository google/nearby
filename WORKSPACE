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

_ALL_CONTENT = """\
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)
"""

http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-20230802.1",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.1.zip"],
)

# Using a protobuf javalite version that contains @com_google_protobuf_javalite//:javalite_toolchain
http_archive(
    name = "com_google_protobuf_javalite",
    strip_prefix = "protobuf-javalite",
    urls = ["https://github.com/google/protobuf/archive/javalite.zip"],
)

http_archive(
    name = "com_google_protobuf",
    strip_prefix = "protobuf-3.17.0",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.17.0.tar.gz"],
)

http_archive(
    name = "com_google_protobuf_cc",
    strip_prefix = "protobuf-3.17.0",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.17.0.tar.gz"],
)

http_archive(
    name = "com_google_protobuf_java",
    strip_prefix = "protobuf-3.17.0",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.17.0.tar.gz"],
)

http_archive(
    name = "com_google_glog",
    sha256 = "f28359aeba12f30d73d9e4711ef356dc842886968112162bc73002645139c39c",
    strip_prefix = "glog-0.4.0",
    urls = ["https://github.com/google/glog/archive/v0.4.0.tar.gz"],
)

http_archive(
    name = "com_google_ukey2",
    strip_prefix = "ukey2-master",
    urls = ["https://github.com/google/ukey2/archive/master.zip"],
)

http_archive(
    name = "aappleby_smhasher",
    strip_prefix = "smhasher-master",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "libmurmur3",
    srcs = ["src/MurmurHash3.cpp"],
    hdrs = ["src/MurmurHash3.h"],
    copts = ["-Wno-implicit-fallthrough"],
    licenses = ["unencumbered"],  # MurmurHash is explicity public-domain
)""",
    urls = ["https://github.com/aappleby/smhasher/archive/master.zip"],
)

http_archive(
    name = "nlohmann_json",
    strip_prefix = "json-3.10.5",
    build_file_content = """
cc_library(
  name = "json",
  hdrs = glob([
    "include/nlohmann/**/*.hpp",
  ]),
  includes = ["include"],
  visibility = ["//visibility:public"],
  alwayslink = True,
)""",
    urls = [
        "https://github.com/nlohmann/json/archive/refs/tags/v3.10.5.tar.gz",
    ],
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
# Load common dependencies.
protobuf_deps()

http_archive(
    name = "com_google_googletest",
    strip_prefix = "googletest-main",
    urls = ["https://github.com/google/googletest/archive/main.zip"],
)

http_archive(
    name = "com_google_webrtc",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
""",
    urls = ["https://webrtc.googlesource.com/src/+archive/main.tar.gz"],
)

# gflags needed by glog
http_archive(
    name = "com_github_gflags_gflags",
    strip_prefix = "gflags-2.2.2",
    sha256 = "19713a36c9f32b33df59d1c79b4958434cb005b5b47dc5400a7a4b078111d9b5",
    url = "https://github.com/gflags/gflags/archive/v2.2.2.zip",
)

# ----------------------------------------------
# Nisaba: Script processing library from Google:
# ----------------------------------------------
# We depend on some of core C++ libraries from Nisaba and use the fresh code
# from the HEAD. See
#   https://github.com/google-research/nisaba

nisaba_version = "main"

http_archive(
    name = "com_google_nisaba",
    url = "https://github.com/google-research/nisaba/archive/refs/heads/%s.zip" % nisaba_version,
    strip_prefix = "nisaba-%s" % nisaba_version,
)

load("@com_google_nisaba//bazel:workspace.bzl", "nisaba_public_repositories")

nisaba_public_repositories()
http_archive(
    name = "boringssl",
    sha256 = "5d299325d1db8b2f2db3d927c7bc1f9fcbd05a3f9b5c8239fa527c09bf97f995",  # Last updated 2022-10-19
    strip_prefix = "boringssl-0acfcff4be10514aacb98eb8ab27bb60136d131b",
    urls = ["https://github.com/google/boringssl/archive/0acfcff4be10514aacb98eb8ab27bb60136d131b.tar.gz"],
)
# -------------------------------------------------------------------------
# Protocol buffer matches (should be part of gmock and gtest, but not yet):
#   https://github.com/inazarenko/protobuf-matchers

http_archive(
    name = "com_github_protobuf_matchers",
    urls = ["https://github.com/inazarenko/protobuf-matchers/archive/refs/heads/master.zip"],
    strip_prefix = "protobuf-matchers-master",
)

http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "26155e050b10b5969e986dab35654247a3b1b295e0532880b5a9c13c0a700ceb",
    strip_prefix = "re2-2021-06-01",
    urls = [
        "https://github.com/google/re2/archive/refs/tags/2021-06-01.tar.gz",
    ],
)
