add_library(platform_impl_shared_posix_lock STATIC)

target_sources(platform_impl_shared_posix_lock
  PRIVATE
    "posix_lock.cc"
  PUBLIC
    "posix_lock.h"
)

target_include_directories(platform_impl_shared_posix_lock
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(platform_impl_shared_posix_lock
  PUBLIC
    platform_api
    platform_types
)

add_library(platform_impl_shared_posix_condition_variable STATIC)

target_sources(platform_impl_shared_posix_condition_variable
  PRIVATE
    "posix_condition_variable.cc"
  PUBLIC
    "posix_condition_variable.h"
)

target_include_directories(platform_impl_shared_posix_condition_variable
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_shared_posix_condition_variable
  PUBLIC
    platform_api
    platform_impl_shared_posix_lock
    platform_types
    absl::raw_logging_internal
)

add_library(platform_impl_shared_file STATIC)

target_sources(platform_impl_shared_file
  PRIVATE
    "file_impl.cc"
  PUBLIC
    "file_impl.h"
)

target_include_directories(platform_impl_shared_file
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_shared_file
  PUBLIC
    platform_api
    platform_types
)

add_library(platform_impl_shared_atomic_boolean STATIC)

target_sources(platform_impl_shared_atomic_boolean
  PUBLIC
    "atomic_boolean_impl.h"
)

target_include_directories(platform_impl_shared_atomic_boolean
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(platform_impl_shared_atomic_boolean
  PUBLIC
    platform_api
    platform_types
)

set_target_properties(platform_impl_shared_atomic_boolean
  PROPERTIES
    LINKER_LANGUAGE CXX
)

add_subdirectory(sample)
