add_library(platform_port_config_private INTERFACE)

target_sources(platform_port_config_private
  INTERFACE
    config.h
)

add_library(platform_port_string INTERFACE)

target_sources(platform_port_string
  INTERFACE
    string.h
)

target_link_libraries(platform_port_string
  INTERFACE
    platform_port_config_private
)

add_library(platform_port_down_cast INTERFACE)

target_sources(platform_port_down_cast
  INTERFACE
    down_cast.h
)

target_link_libraries(platform_port_down_cast
  INTERFACE
    platform_port_config_private
)
