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

add_library(core_internal STATIC)

target_sources(core_internal
  PRIVATE
    bandwidth_upgrade_manager.cc
    base_bandwidth_upgrade_handler.cc
    base_endpoint_channel.cc
    ble_advertisement.cc
    ble_endpoint_channel.cc
    bluetooth_device_name.cc
    bluetooth_endpoint_channel.cc
    endpoint_channel_manager.cc
    internal_payload.cc
    internal_payload.h
    loop_runner.cc
    loop_runner.h
    offline_frames.cc
    wifi_lan_service_info.cc
    wifi_lan_endpoint_channel.cc
  PUBLIC
    bandwidth_upgrade_handler.h
    bandwidth_upgrade_manager.h
    base_bandwidth_upgrade_handler.h
    base_endpoint_channel.h
    base_pcp_handler.h
    ble_advertisement.h
    ble_compat.h
    ble_endpoint_channel.h
    bluetooth_device_name.h
    bluetooth_endpoint_channel.h
    client_proxy.h
    encryption_runner.h
    endpoint_channel.h
    endpoint_channel_manager.h
    endpoint_manager.h
    internal_payload_factory.h
    medium_manager.h
    offline_frames.h
    offline_service_controller.h
    p2p_cluster_pcp_handler.h
    p2p_point_to_point_pcp_handler.h
    p2p_star_pcp_handler.h
    payload_manager.h
    pcp.h
    pcp_handler.h
    pcp_manager.h
    service_controller.h
    service_controller_router.h
    wifi_lan_endpoint_channel.h
    wifi_lan_upgrade_handler.h
)

target_link_libraries(core_internal
  PUBLIC
    absl::strings
    core_internal_mediums
    core_types
    platform_api
    platform_port_down_cast
    platform_port_string
    platform_types
    platform_utils
    proto_connections_enums_cc_proto
    proto_offline_wire_formats_cc_proto
    ukey2
)

add_executable(core_internal_test
    base_endpoint_channel_test.cc
    bluetooth_device_name_test.cc
    ble_advertisement_test.cc
    offline_frames_test.cc
    wifi_lan_service_info_test.cc
)

add_test(
  NAME core_internal_test
  COMMAND core_internal_test
)

target_link_libraries(core_internal_test
  PUBLIC
    core_internal
    gmock
    gtest
    gtest_main
    platform_impl_g3
    platform_impl_shared_posix_condition_variable
    platform_impl_shared_posix_lock
    platform_port_string
    platform_utils
)

add_subdirectory(mediums)
