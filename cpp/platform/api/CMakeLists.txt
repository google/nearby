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

add_library(platform_api STATIC
    atomic_boolean.h
    atomic_reference.h
    ble.h
    ble_v2.h
    bluetooth_adapter.h
    bluetooth_classic.h
    condition_variable.h
    count_down_latch.h
    executor.h
    future.h
    hash_utils.h
    input_file.h
    input_stream.h
    listenable_future.h
    lock.h
    multi_thread_executor.h
    output_file.h
    output_stream.h
    scheduled_executor.h
    server_sync.h
    settable_future.h
    single_thread_executor.h
    socket.h
    submittable_executor.h
    system_clock.h
    thread_utils.h
    webrtc.h
    wifi.h
    wifi_lan.h
)

target_link_libraries(platform_api
  PUBLIC
    platform_types
)
