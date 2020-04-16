# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
