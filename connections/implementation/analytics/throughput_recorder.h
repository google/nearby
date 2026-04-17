// Copyright 2022-2023 Google LLC
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
#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/packet_meta_data.h"
#include "connections/payload_type.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace analytics {

// Container class to manage ThroughputRecorder instances.
// This class is a singleton and provides thread-safe proxy methods to record
// throughput for different payloads.
class ThroughputRecorderContainer {
 public:
  static ThroughputRecorderContainer& GetInstance();

  // Records the start of a payload transfer.
  void Start(int64_t payload_id,
             connections::PayloadDirection payload_direction,
             connections::PayloadType payload_type) ABSL_LOCKS_EXCLUDED(mutex_);

  // Records when a frame is sent or received.
  void UpdateFrameData(int64_t payload_id,
                       connections::PayloadDirection payload_direction,
                       location::nearby::proto::connections::Medium medium,
                       PacketMetaData& packet_meta_data)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Marks a payload transfer as successful.
  void MarkAsSuccess(int64_t payload_id,
                     connections::PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops and removes the throughput recorder for a given payload.
  // This calculates and logs the final throughput statistics.
  // Returns the throughput in KBps.
  int64_t StopTPRecorder(int64_t payload_id,
                         connections::PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the number of active recorder instances.
  int GetSize() ABSL_LOCKS_EXCLUDED(mutex_);

  // Clear all recorders. Used for testing.
  void ClearForTest() ABSL_LOCKS_EXCLUDED(mutex_);

  // Testing proxy methods
  int64_t GetTotalByteSizeForTesting(
      int64_t payload_id, connections::PayloadDirection payload_direction,
      location::nearby::proto::connections::Medium medium)
      ABSL_LOCKS_EXCLUDED(mutex_);
  int GetThroughputsSizeForTesting(
      int64_t payload_id, connections::PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);
  int64_t GetDurationMillisForTesting(
      int64_t payload_id, connections::PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);
  int64_t GetThroughputKbpsForTesting(
      int64_t payload_id, connections::PayloadDirection payload_direction)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool DumpForTesting(int64_t payload_id,
                      connections::PayloadDirection payload_direction,
                      location::nearby::proto::connections::Medium medium)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class absl::NoDestructor<ThroughputRecorderContainer>;

  class ThroughputRecorder {
   public:
    ThroughputRecorder(int64_t payload_id,
                       connections::PayloadDirection payload_direction,
                       connections::PayloadType payload_type);
    ~ThroughputRecorder() = default;

    void Start();
    bool Stop() ABSL_LOCKS_EXCLUDED(mutex_);

    class Throughput {
     public:
      Throughput() = default;
      ~Throughput() = default;
      Throughput(location::nearby::proto::connections::Medium medium,
                 absl::Time start_timestamp)
          : medium_(medium), start_timestamp_(start_timestamp) {}

      void Add(int frame_size, int64_t file_io_time, int64_t encryption_time,
               int64_t socket_io_time);

      void SetLastTimestamp(absl::Time time_stamp) {
        last_timestamp_ = time_stamp;
      }

      int64_t GetTotalByteSize() const { return total_byte_size_; }

      bool dump(connections::PayloadDirection payload_direction,
                connections::PayloadType payload_type);

     private:
      const ::location::nearby::proto::connections::Medium medium_;
      const absl::Time start_timestamp_;
      int64_t total_byte_size_ = 0;
      absl::Time last_timestamp_;
      int64_t file_io_time_ = 0;
      int64_t encryption_time_ = 0;
      int64_t socket_io_time_ = 0;
    };

    Throughput& GetThroughput(
        location::nearby::proto::connections::Medium medium,
        int64_t duration_millis);
    int GetThroughputsSize() const;
    int64_t GetThroughputKbps() const;
    int64_t GetDurationMillis() const;
    void UpdateFrameData(location::nearby::proto::connections::Medium medium,
                         PacketMetaData& packetMetaData);
    void MarkAsSuccess();
    connections::PayloadType GetPayloadType() const { return payload_type_; }

   private:
    void CalculateDurationTimes(const PacketMetaData& packetMetaData);

    const int64_t payload_id_;
    const connections::PayloadDirection payload_direction_;
    const connections::PayloadType payload_type_;
    absl::Time start_timestamp_;
    absl::flat_hash_map<location::nearby::proto::connections::Medium,
                        Throughput>
        throughputs_;
    bool success_ = false;

    int64_t file_io_time_ = 0;
    int64_t encryption_time_ = 0;
    int64_t socket_io_time_ = 0;
    int64_t duration_millis_ = 0;
    int64_t throughput_kbps_ = 0;
  };

  ThroughputRecorderContainer() = default;
  ThroughputRecorderContainer(const ThroughputRecorderContainer&) = delete;
  ThroughputRecorderContainer& operator=(const ThroughputRecorderContainer&) =
      delete;
  ~ThroughputRecorderContainer() = default;

  Mutex mutex_;
  // std::pair<int64_t, PayloadDirection> for <payload id, payload direction>
  absl::flat_hash_map<std::pair<int64_t, connections::PayloadDirection>,
                      std::unique_ptr<ThroughputRecorder>>
      throughput_recorders_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace analytics
}  // namespace nearby

#endif  // NEARBY_CONNECTIONS_IMPLEMENTATION_ANALYTICS_THROUGHPUT_RECORDER_H_
