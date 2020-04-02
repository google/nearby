add_library(core STATIC)

target_sources(core
  PUBLIC
    core.h
)

target_include_directories(core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/cpp
)

target_link_libraries(core
  PUBLIC
    core_internal
    platform_types
)

add_library(core_types STATIC)

target_sources(core_types
  PRIVATE
    payload.cc
    strategy.cc
  PUBLIC
    listeners.h
    options.h
    params.h
    payload.h
    status.h
    strategy.h
)

target_link_libraries(core_types
  PUBLIC
    platform_api
    platform_port_string
    platform_types
    platform_utils
)

add_executable(core_build_test
    check_compilation.cc
)

target_link_libraries(core_build_test
  PUBLIC
    absl::strings
    core
    core_types
    platform_impl_default_lock
    platform_impl_sample
    platform_port_string
    platform_types
    platform_utils
)

add_subdirectory(internal)
