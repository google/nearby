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
