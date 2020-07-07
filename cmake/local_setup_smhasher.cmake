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

set(PKG_STAGE_SRC_ROOT ${TOOLS_ROOT}/src/smhasher)
if (NOT EXISTS ${PKG_STAGE_SRC_ROOT}/CMakeLists.txt)
  set(PKG_SRC_ROOT ${PROJECT_SOURCE_DIR}/third_party/smhasher)
  execute_process(
    COMMAND mkdir -p ${PKG_STAGE_SRC_ROOT}/cpp/src/smhasher/src
  )
  execute_process(
    COMMAND mkdir -p ${PKG_STAGE_SRC_ROOT}/cpp/include/smhasher/src
  )
  execute_process(
    COMMAND cp ${PKG_SRC_ROOT}/src/MurmurHash3.cpp ${PKG_STAGE_SRC_ROOT}/cpp/src/smhasher/src
  )
  execute_process(
    COMMAND cp ${PKG_SRC_ROOT}/src/MurmurHash3.h ${PKG_STAGE_SRC_ROOT}/cpp/include/smhasher/src
  )
  execute_process(
    COMMAND cp cmake/CMakeLists-smhasher.txt ${PKG_STAGE_SRC_ROOT}/CMakeLists.txt
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endif()

add_subdirectory(${PKG_STAGE_SRC_ROOT})
