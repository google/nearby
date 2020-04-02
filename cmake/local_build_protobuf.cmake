if (NOT EXISTS ${TOOLS_INSTALL_PREFIX}/bin/protoc)
  set(PKG_BUILD_ROOT ${TOOLS_BUILD_ROOT}/protobuf)
  set(PKG_SRC_ROOT ${CMAKE_SOURCE_DIR}/third_party/protobuf)
  execute_process(
    COMMAND mkdir -p ${PKG_BUILD_ROOT}
  )
  execute_process(
    COMMAND cmake ${PKG_SRC_ROOT}/cmake
    WORKING_DIRECTORY ${PKG_BUILD_ROOT}
  )
  execute_process(
    COMMAND make -j${N_CPUS}
    WORKING_DIRECTORY ${PKG_BUILD_ROOT}
  )
  execute_process(
    COMMAND make check
    WORKING_DIRECTORY ${PKG_BUILD_ROOT}
    RESULT_VARIABLE test_exit_code
    ERROR_QUIET
  )
  if (NOT ${test_exit_code} EQUAL "0")
    message(FATAL_ERROR "Protobuf tests failed; can't use this protobuf")
  endif()
  execute_process(
    COMMAND /bin/bash -c "DESTDIR=${TOOLS_INSTALL_ROOT} make install"
    WORKING_DIRECTORY ${PKG_BUILD_ROOT}
  )
endif()
