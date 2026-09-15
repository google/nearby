include_directories("${CMAKE_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

get_target_property(PRESENCE_DIR presence BINARY_DIR)
set(LIB_ENGINE_A ${PRESENCE_DIR}/libpresence.a)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE ${LIB_ENGINE_A} spdlog::spdlog)
add_dependencies(main presence)