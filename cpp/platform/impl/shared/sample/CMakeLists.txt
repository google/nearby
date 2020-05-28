add_library(platform_impl_shared_sample STATIC)

target_sources(platform_impl_shared_sample
  PRIVATE
    sample_wifi_medium.cc
  PUBLIC
    sample_wifi_medium.h
)

target_link_libraries(platform_impl_shared_sample
  PUBLIC
    absl::time
    platform_api
    platform_port_string
    platform_types
    platform_utils
)
