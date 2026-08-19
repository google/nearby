cmake_minimum_required(VERSION 3.14...3.28)
project(presence)

enable_testing()

# For logging support.
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG         v1.14.1
)
FetchContent_MakeAvailable(spdlog)

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
set_target_properties(presence PROPERTIES BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Tests in Rust can only be invoked here due to the dependencies on C codes.
add_test(NAME presence_ffi_rust_test
    COMMAND cargo test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(presence_ffi_rust_test PROPERTIES DEPENDS presence)
set_tests_properties(presence_ffi_rust_test PROPERTIES
        ENVIRONMENT CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(tests)

add_subdirectory(examples)
