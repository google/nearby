cmake_minimum_required(VERSION 3.14...3.28)
project(presence)

enable_testing()

add_subdirectory(cpp)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build)
    set(TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release)
    set(TARGET_DIR "release")
endif ()

set(PRESENCE_ENGINE_A "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIR}/libpresence.a")

add_custom_target(presence ALL
    COMMENT "Compiling Presence Engine module"
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} ${CARGO_CMD}
    COMMAND cp ${PRESENCE_ENGINE_A} ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(presence cpp_ffi)
set_target_properties(presence PROPERTIES BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME presence_engine_test
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

#add_subdirectory(tests)
add_subdirectory(examples)
