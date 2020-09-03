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
        "//core_v2/internal:__subpackages__",
    ],
    deps = [
        "//core_v2:core_types",
        "//platform_v2/base",
        "//platform_v2/base:util",
        "//platform_v2/public:logging",
        "//platform_v2/public:types",
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
        "//platform_v2/base",
        "//platform_v2/impl/g3",  # buildcleaner: keep
        "//testing/base/public:gunit_main",
        "//absl/time",
    ],
)
