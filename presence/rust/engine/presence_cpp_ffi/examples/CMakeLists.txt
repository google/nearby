# For logging support.
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG         v1.14.1
)
FetchContent_MakeAvailable(spdlog)

include_directories("${CMAKE_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

get_target_property(PRESENCE_DIR presence BINARY_DIR)
set(LIB_ENGINE_A ${PRESENCE_DIR}/libpresence.a)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE ${LIB_ENGINE_A} spdlog::spdlog)
add_dependencies(main presence)