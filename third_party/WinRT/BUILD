load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# C++/WinRT projection headers ship with the Windows SDK and are already on the
# MSVC/clang-cl INCLUDE path; this target only supplies the umbrella library.
cc_library(
    name = "winrt",
    linkopts = ["windowsapp.lib"],
)
