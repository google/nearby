cc_library(
    name = "mediums",
    srcs = [
        "advertisement_read_result.cc",
        "ble_advertisement.cc",
        "ble_advertisement_header.cc",
        "ble_packet.cc",
        "bluetooth_radio.cc",
        "uuid.cc",
    ],
    hdrs = [
        "advertisement_read_result.h",
        "ble_advertisement.h",
        "ble_advertisement_header.h",
        "ble_packet.h",
        "ble_peripheral.h",
        "bluetooth_radio.h",
        "lost_entity_tracker.h",
        "uuid.h",
    ],
    visibility = [
        "//core_v2/internal:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//absl/container:flat_hash_map",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//absl/time",
    ],
)

cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    visibility = [
        "//core_v2/internal/mediums/webrtc:__pkg__",
    ],
    deps = [
        "//platform_v2/base",
        "//platform_v2/public",
    ],
)

cc_test(
    name = "core_v2_internal_mediums_test",
    srcs = [
        "advertisement_read_result_test.cc",
        "ble_advertisement_header_test.cc",
        "ble_advertisement_test.cc",
        "ble_packet_test.cc",
        "ble_peripheral_test.cc",
        "bluetooth_radio_test.cc",
        "lost_entity_tracker_test.cc",
        "uuid_test.cc",
    ],
    shard_count = 16,
    deps = [
        ":mediums",
        "//platform_v2/base",
        "//platform_v2/impl/g3",  # build_cleaner: keep
        "//platform_v2/public",
        "//platform_v2/public:logging",
        "//testing/base/public:gunit_main",
        "//absl/time",
    ],
)
