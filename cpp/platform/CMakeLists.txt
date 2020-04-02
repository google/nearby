add_library(platform_utils STATIC
  base64_utils.cc
  file_impl.cc
  prng.cc
  reliability_utils.cc
)

target_sources(platform_utils
  PUBLIC
    base64_utils.h
    cancelable_alarm.h
    file_impl.h
    pipe.h
    prng.h
    reliability_utils.h
    synchronized.h
)

target_link_libraries(platform_utils
  PUBLIC
    platform_types
    platform_api
    absl::strings
)

add_library(platform_types STATIC
  ptr.cc
)

target_sources(platform_types
  PUBLIC
    byte_array.h
    callable.h
    cancelable.h
    container_of.h
    exception.h
    logging.h
    ptr.h
    runnable.h
)

target_link_libraries(platform_types
  PUBLIC
    absl::base
    absl::strings
)

add_executable(platform_test
  byte_array_test.cc
  container_of_test.cc
  file_impl_test.cc
  pipe_test.cc
  prng_test.cc
  ptr_test.cc
)

target_link_libraries(platform_test
  PUBLIC
    absl::base
    absl::strings
    absl::time
    gtest
    gtest_main
    platform_api
    platform_impl_default_cond_var
    platform_impl_default_lock
    platform_types
    platform_utils
)

add_test(
  NAME platform_test
  COMMAND platform_test
)

add_subdirectory(api)
add_subdirectory(impl/sample)
add_subdirectory(impl/default)
add_subdirectory(port)
