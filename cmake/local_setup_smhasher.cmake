set(PKG_STAGE_SRC_ROOT ${TOOLS_ROOT}/src/smhasher)
if (NOT EXISTS ${PKG_STAGE_SRC_ROOT}/CMakeLists.txt)
  set(PKG_SRC_ROOT ${PROJECT_SOURCE_DIR}/third_party/smhasher)
  execute_process(
    COMMAND mkdir -p ${PKG_STAGE_SRC_ROOT}/cpp/src/smhasher
  )
  execute_process(
    COMMAND mkdir -p ${PKG_STAGE_SRC_ROOT}/cpp/include/smhasher
  )
  execute_process(
    COMMAND cp ${PKG_SRC_ROOT}/src/MurmurHash3.cpp ${PKG_STAGE_SRC_ROOT}/cpp/src/smhasher
  )
  execute_process(
    COMMAND cp ${PKG_SRC_ROOT}/src/MurmurHash3.h ${PKG_STAGE_SRC_ROOT}/cpp/include/smhasher
  )
  execute_process(
    COMMAND cp cmake/CMakeLists-smhasher.txt ${PKG_STAGE_SRC_ROOT}/CMakeLists.txt
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endif()

add_subdirectory(${PKG_STAGE_SRC_ROOT})
