cc_library(
    name = "ble_v2",
    srcs = [
        "advertisement_read_result.cc",
        "ble_advertisement.cc",
        "ble_advertisement_header.cc",
        "ble_packet.cc",
    ],
    hdrs = [
        "advertisement_read_result.h",
        "ble_advertisement.h",
        "ble_advertisement_header.h",
        "ble_packet.h",
        "ble_peripheral.h",
        "discovered_peripheral_callback.h",
    ],
    visibility = [
        "//core/internal:__subpackages__",
    ],
    deps = [
        "//core:core_types",
        "//platform/base",
        "//platform/base:util",
        "//platform/public:logging",
        "//platform/public:types",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//absl/time",
    ],
)

cc_test(
    name = "ble_v2_test",
    srcs = [
        "advertisement_read_result_test.cc",
        "ble_advertisement_header_test.cc",
        "ble_advertisement_test.cc",
        "ble_packet_test.cc",
        "ble_peripheral_test.cc",
    ],
    deps = [
        ":ble_v2",
        "//platform/base",
        "//platform/impl/g3",  # buildcleaner: keep
        "//testing/base/public:gunit_main",
        "//absl/time",
    ],
)
