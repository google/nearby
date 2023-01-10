// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_PACKET_META_DATA_H_
#define NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_PACKET_META_DATA_H_

#include <cstdint>
#include <string>

#include "absl/time/time.h"
#include "internal/platform/system_clock.h"

namespace nearby {
namespace analytics {

struct PacketMetaData {
  int packet_size;
  absl::Time file_io_start_time;
  absl::Time file_io_end_time;
  absl::Time encryption_start_time;
  absl::Time encryption_end_time;
  absl::Time socket_io_start_time;
  absl::Time socket_io_end_time;

  void Reset() {
    file_io_start_time = SystemClock::ElapsedRealtime();
    socket_io_start_time = SystemClock::ElapsedRealtime();
    socket_io_start_time = SystemClock::ElapsedRealtime();
    packet_size = 0;
  }

  void SetPacketSize(int packet_size) {
    this->packet_size = packet_size;
  }

  int GetPacketSize() {
    return packet_size;
  }

  void StartFileIo() {
    file_io_start_time = SystemClock::ElapsedRealtime();
  }

  void StopFileIo() {
    file_io_end_time = SystemClock::ElapsedRealtime();
  }

  void StartEncryption() {
    encryption_start_time = SystemClock::ElapsedRealtime();
  }

  void StopEncryption() {
    encryption_end_time = SystemClock::ElapsedRealtime();
  }

  void StartSocketIo() {
    socket_io_start_time = SystemClock::ElapsedRealtime();
  }

  void StopSocketIo() {
    socket_io_end_time = SystemClock::ElapsedRealtime();
  }

  int64_t GetEncryptionTimeInMillis() {
    if (encryption_end_time > encryption_start_time) {
      return absl::ToInt64Milliseconds(encryption_end_time -
                                       encryption_start_time);
    }
    return 0L;
  }

  int64_t GetFileIoTimeInMillis() {
    if (file_io_end_time > file_io_start_time) {
      return absl::ToInt64Milliseconds(file_io_end_time - file_io_start_time);
    }
    return 0L;
  }

  int64_t GetSocketIoTimeInMillis() {
    if (socket_io_end_time > socket_io_start_time) {
      return absl::ToInt64Milliseconds(socket_io_end_time -
                                       socket_io_start_time);
    }
    return 0L;
  }
};

}  // namespace analytics
}  // namespace nearby

#endif  // NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_PACKET_META_DATA_H_
