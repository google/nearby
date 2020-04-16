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

add_library(platform_port_config_private INTERFACE)

target_sources(platform_port_config_private
  INTERFACE
    config.h
)

add_library(platform_port_string INTERFACE)

target_sources(platform_port_string
  INTERFACE
    string.h
)

target_link_libraries(platform_port_string
  INTERFACE
    platform_port_config_private
)

add_library(platform_port_down_cast INTERFACE)

target_sources(platform_port_down_cast
  INTERFACE
    down_cast.h
)

target_link_libraries(platform_port_down_cast
  INTERFACE
    platform_port_config_private
)
