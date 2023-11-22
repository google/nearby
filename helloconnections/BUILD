load("//tools/build_defs/android:rules.bzl", "android_binary")

android_binary(
    name = "helloconnections",
    srcs = glob(["*.java"]),
    manifest = "AndroidManifest.xml",
    multidex = "native",
    resource_files = glob(["res/**"]),
    visibility = ["//java/com/google/location/nearby/apps:__pkg__"],
    deps = [
        "//java/com/google/android/gmscore/integ/client/base",
        "//java/com/google/android/gmscore/integ/client/common",
        "//java/com/google/android/gmscore/integ/client/nearby",
        "//third_party/java/android/android_sdk_linux/extras/android/compatibility/v4",
        "//third_party/java/android_libs/guava_jdk5:collect",
        "//third_party/java/android_libs/guava_jdk5:io",
        "//third_party/java/android_libs/guava_jdk5:primitives",
        "//third_party/java/androidx/appcompat",
        "//third_party/java/androidx/recyclerview",
        "//third_party/java/auto:auto_value",
    ],
)
