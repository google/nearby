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

#ifndef NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_THROUGHPUT_RECORDER_H_
#define NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_THROUGHPUT_RECORDER_H_

#include <cstdint>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "connections/implementation/analytics/packet_meta_data.h"
#include "connections/payload_type.h"
#include "internal/platform/mutex.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace analytics {

// The following aliases are only for users' convenience.
using ::location::nearby::proto::connections::Medium;
using ::nearby::connections::PayloadType;
// Enum to represent if a payload is incoming or outgoing.
using ::nearby::connections::PayloadDirection;

class ThroughputRecorder {
 public:
  explicit ThroughputRecorder(int64_t payload_id);
  ~ThroughputRecorder() = default;

  void Start(PayloadType payload_type, PayloadDirection payload_direction);
  bool Stop() ABSL_LOCKS_EXCLUDED(mutex_);
  static int CalculateThroughputKBps(int64_t total_byte_size,
                                     int64_t total_millis);
  static int CalculateThroughputMBps(int throughputKBps);

  class Throughput {
   public:
    Throughput() = default;
    ~Throughput() = default;
    Throughput(Medium medium, absl::Time start_timestamp,
               PayloadType payload_type, PayloadDirection payload_direction)
        : medium_(medium),
          start_timestamp_(start_timestamp),
          payload_type_(payload_type),
          payload_direction_(payload_direction) {}

    void Add(int frame_size, int64_t file_io_time, int64_t encryption_time,
             int64_t socket_io_time);

    void SetLastTimestamp(absl::Time time_stamp) {
      last_timestamp_ = time_stamp;
    }

    int64_t GetTotalByteSize() { return total_byte_size_; }

    bool dump();

   private:
    Medium medium_;
    absl::Time start_timestamp_;
    PayloadType payload_type_;
    int64_t total_byte_size_ = 0;
    absl::Time last_timestamp_;
    PayloadDirection payload_direction_ = PayloadDirection::INCOMING_PAYLOAD;
    int64_t file_io_time_ = 0;
    int64_t encryption_time_ = 0;
    int64_t socket_io_time_ = 0;
  };

  Throughput& GetThroughput(Medium medium, int64_t duration_millis);
  int GetThroughputsSize();
  int GetThroughputKbps();
  int64_t GetDurationMillis();
  void OnFrameSent(Medium medium, PacketMetaData& packetMetaData);
  void OnFrameReceived(Medium medium, PacketMetaData& packetMetaData);
  void MarkAsSuccess() { success_ = true; }

 private:
  void CalculateDurationTimes(PacketMetaData packetMetaData);
  static std::string ToString(PayloadType type);

  Mutex mutex_;
  int64_t payload_id_ = 0;
  absl::Time start_timestamp_;
  PayloadType payload_type_ = PayloadType::kUnknown;
  PayloadDirection payload_direction_ = PayloadDirection::INCOMING_PAYLOAD;
  absl::flat_hash_map<Medium, Throughput> throughputs_;
  bool success_ = false;

  int64_t file_io_time_ = 0;
  int64_t encryption_time_ = 0;
  int64_t socket_io_time_ = 0;
  int64_t duration_millis_ = 0;
  int throughput_kbps_ = 0;
};

class ThroughputRecorderContainer {
 public:
  ThroughputRecorderContainer(const ThroughputRecorderContainer&) = delete;
  ThroughputRecorderContainer& operator=(const ThroughputRecorderContainer&) =
      delete;

  static ThroughputRecorderContainer& GetInstance();
  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_);

  ThroughputRecorder* GetTPRecorder(int64_t payload_id,
                                    PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StopTPRecorder(int64_t payload_id, PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);
  int GetSize() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // This is a singleton object, for which destructor will never be called.
  // Constructor will be invoked once from Instance() static method.
  // Object is create in-place (with a placement new) to guarantee that
  // destructor is not scheduled for execution at exit.
  ThroughputRecorderContainer() = default;
  ~ThroughputRecorderContainer() = default;

  Mutex mutex_;
  // std::pair<int64_t, PayloadDirection> for <payload id, payload direction>
  absl::flat_hash_map<std::pair<int64_t, PayloadDirection>, ThroughputRecorder*>
      throughput_recorders_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace analytics
}  // namespace nearby

#endif  // NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_THROUGHPUT_RECORDER_H_
