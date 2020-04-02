add_cc_proto_library(
  proto_bootstrap_enums_cc_proto
  PROTOS bootstrap_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_connections_enums_cc_proto
  PROTOS connections_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_discovery_enums_cc_proto
  PROTOS discovery_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_magic_pair_enums_cc_proto
  PROTOS magic_pair_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_nearby_client_enums_cc_proto
  PROTOS nearby_client_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_nearby_event_codes_cc_proto
  PROTOS nearby_event_codes.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_setup_enums_cc_proto
  PROTOS setup_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_cc_proto_library(
  proto_sharing_enums_cc_proto
  PROTOS sharing_enums.proto
  INCS ${CMAKE_CURRENT_BINARY_DIR}/..
)

add_subdirectory(connections)
