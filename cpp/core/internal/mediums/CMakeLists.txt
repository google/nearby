# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
    platform_impl_g3
    platform_utils
)

add_test(
  NAME core_internal_mediums_test
  COMMAND core_internal_mediums_test
)
