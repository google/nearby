include(ProcessorCount)
ProcessorCount(N_CPUS)

if (N_CPUS EQUAL 0)
  set (N_CPUS 1)
endif()

set (TOOLS_ROOT ${CMAKE_BINARY_DIR}/stage)
set (TOOLS_BUILD_ROOT ${TOOLS_ROOT}/build)
set (TOOLS_INSTALL_ROOT ${TOOLS_ROOT}/install)
set (TOOLS_INSTALL_PREFIX ${TOOLS_INSTALL_ROOT}/usr/local)
set (CMAKE_FIND_ROOT_PATH ${TOOLS_INSTALL_ROOT})
