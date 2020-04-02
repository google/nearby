add_library(core_internal_mediums STATIC)

target_sources(core_internal_mediums
  PRIVATE
    ble_advertisement.cc
    ble_advertisement_header.cc
    ble_packet.cc
    ble_peripheral.cc
    utils.cc
    utils.h
  PUBLIC
    advertisement_read_result.h
    ble.h
    ble_advertisement.h
    ble_advertisement_header.h
    ble_packet.h
    ble_peripheral.h
    ble_v2.h
    bloom_filter.h
    bluetooth_classic.h
    bluetooth_radio.h
    discovered_peripheral_callback.h
    discovered_peripheral_tracker.h
    lost_entity_tracker.h
    mediums.h
    uuid.h
)

target_link_libraries(core_internal_mediums
  PUBLIC
    absl::numeric
    absl::strings
    platform_api
    platform_port_string
    platform_types
    platform_utils
    smhasher_murmur3
)

add_executable(core_internal_mediums_test
  advertisement_read_result_test.cc
  ble_advertisement_header_test.cc
  ble_advertisement_test.cc
  ble_packet_test.cc
  bloom_filter_test.cc
  lost_entity_tracker_test.cc
)

target_link_libraries(core_internal_mediums_test
  PUBLIC
    absl::time
    core_internal_mediums
    gtest
    gtest_main
    platform_impl_default
    platform_utils
)

add_test(
  NAME core_internal_mediums_test
  COMMAND core_internal_mediums_test
)
