add_library(core_internal STATIC)

target_sources(core_internal
  PRIVATE
    ble_advertisement.cc
    bluetooth_device_name.cc
    internal_payload.cc
    internal_payload.h
    loop_runner.cc
    loop_runner.h
    offline_frames.cc
    offline_frames.h
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
    bluetooth_device_name_test.cc
    ble_advertisement_test.cc
)

add_test(
  NAME core_internal_test
  COMMAND core_internal_test
)

target_link_libraries(core_internal_test
  PUBLIC
    core_internal
    gtest
    gtest_main
    platform_impl_default_cond_var
    platform_impl_default_lock
    platform_port_string
    platform_utils
)

add_subdirectory(mediums)
