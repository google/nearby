load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Intel PIE is a proprietary x64-only SDK for Intel Wi-Fi adapters. Windows
# platform code compiles it out with -DNO_INTEL_PIE, so nothing is linked.
cc_library(name = "pie")
