load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])

package(default_visibility = [
    "//visibility:public",
])

cc_library(
    name = "data_manager",
    hdrs = [
        "data_manager.h",
        "data_set.h",
        "leveldb_data_set.h",
        "memory_data_set.h",
    ],
    deps = [
        "//internal/platform:types",
        "//third_party/leveldb:db",
        "//third_party/leveldb:table",
        "//third_party/leveldb:util",
        "//third_party/protobuf:protobuf_lite",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

proto_library(
    name = "leveldb_data_set_test_proto",
    srcs = ["leveldb_data_set_test.proto"],
)

cc_proto_library(
    name = "leveldb_data_set_test_cc_proto",
    deps = [":leveldb_data_set_test_proto"],
)

cc_test(
    name = "data_manager_test",
    size = "small",
    timeout = "short",
    srcs = [
        "leveldb_data_set_test.cc",
        "memory_data_set_test.cc",
    ],
    shard_count = 8,
    deps = [
        ":data_manager",
        ":leveldb_data_set_test_cc_proto",
        "//internal/platform/implementation/g3",  # fixdeps: keep
        "@com_github_protobuf_matchers//protobuf-matchers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
