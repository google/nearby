include(CTest)
include_directories("${CMAKE_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)
FetchContent_Declare(
  catch
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.6)
# CMake 3.14+
FetchContent_MakeAvailable(catch)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
get_target_property(PRESENCE_DIR presence BINARY_DIR)
set(LIB_ENGINE_A ${PRESENCE_DIR}/libpresence.a)

add_executable(integration_test integration_test.cpp)
target_link_libraries(integration_test PRIVATE ${LIB_ENGINE_A} spdlog::spdlog Catch2::Catch2)
add_dependencies(integration_test presence)
add_test(NAME integration_test_cpp COMMAND integration_test)

# For unit test.
add_library(rust_to_c ../cpp/rust_to_c.cpp)

add_executable(unit_tests unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE rust_to_c spdlog::spdlog Catch2::Catch2)
add_dependencies(unit_tests rust_to_c)
add_test(NAME unit_tests_cpp COMMAND unit_tests)
