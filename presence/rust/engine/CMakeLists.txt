if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CARGO_CMD cargo build)
    set(TARGET_DIR "debug")
else ()
    set(CARGO_CMD cargo build --release)
    set(TARGET_DIR "release")
endif ()

set(ENGINE_SO "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_DIR}/libengine.so")

add_custom_target(engine ALL
    COMMENT "Compiling engine module"
    COMMAND CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR} ${CARGO_CMD} 
    COMMAND cp ${ENGINE_SO} ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(engine PROPERTIES BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME rust_engine_test
    COMMAND cargo test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
