include(FetchContent)
include(CTest)
include_directories("${CMAKE_SOURCE_DIR}/engine")

FetchContent_Declare(
  catch
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v2.13.6)
# CMake 3.14+
FetchContent_MakeAvailable(catch)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
get_target_property(ENGINE_DIR engine BINARY_DIR)
set(LIB_ENGINE_SO ${ENGINE_DIR}/libengine.so)

set(SOURCE main.cpp)
add_executable(main ${SOURCE})
target_link_libraries(main ${LIB_ENGINE_SO})
add_dependencies(main engine)

add_executable(test_main test_main.cpp)
target_link_libraries(test_main PRIVATE ${LIB_ENGINE_SO} Catch2::Catch2)
add_dependencies(test_main engine)
add_test(NAME c_engine_test COMMAND test_main)