load("@bazel_pkg_config//:pkg_config.bzl", "pkg_config")

# ================================================ #
# All dependencies have been moved to MODULE.Bazel #
# ================================================ #
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

pkg_config(
    name = "libsystemd",
    pkg_name = "libsystemd",
)

pkg_config(
    name = "libcurl",
    pkg_name = "libcurl",
)

pkg_config(
    name = "sdbus_cpp",
    pkg_name = "sdbus-c++",
)

# gflags needed by glog
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "19713a36c9f32b33df59d1c79b4958434cb005b5b47dc5400a7a4b078111d9b5",
    strip_prefix = "gflags-2.2.2",
    url = "https://github.com/gflags/gflags/archive/v2.2.2.zip",
)

http_archive(
    name = "com_google_glog",
    sha256 = "f28359aeba12f30d73d9e4711ef356dc842886968112162bc73002645139c39c",
    strip_prefix = "glog-0.4.0",
    urls = ["https://github.com/google/glog/archive/v0.4.0.tar.gz"],
)
