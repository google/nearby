include(CTest)
include_directories("${CMAKE_SOURCE_DIR}")

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

add_executable(test_main test_main.cpp)
target_link_libraries(test_main PRIVATE ${LIB_ENGINE_A} spdlog::spdlog Catch2::Catch2)
add_dependencies(test_main presence)
add_test(NAME integration_test_cpp COMMAND test_main)