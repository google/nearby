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

#include "connections/implementation/analytics/throughput_recorder.h"

#include <stdint.h>

#include <new>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace analytics {

namespace {
constexpr int kDefaultThroughoutKbps = 0;
constexpr int kKbInBytes = 1024;
constexpr int kSecInMs = 1000;
}  // namespace

ThroughputRecorder::ThroughputRecorder(int64_t payload_id)
    : payload_id_(payload_id) {}

ThroughputRecorderContainer& ThroughputRecorderContainer::GetInstance() {
  static std::aligned_storage_t<sizeof(ThroughputRecorderContainer),
                                alignof(ThroughputRecorderContainer)>
      storage;
  static ThroughputRecorderContainer* env =
      new (&storage) ThroughputRecorderContainer();
  return *env;
}

void ThroughputRecorder::Start(PayloadType payload_type,
                               PayloadDirection payload_direction) {
  std::string direction =
      (payload_direction == PayloadDirection::INCOMING_PAYLOAD) ? "; Receive"
                                                                : "; Send";

  NEARBY_LOGS(INFO) << "Start TP profiling for payload_id:" << payload_id_
                    << direction;

  if (payload_type == PayloadType::kUnknown) {
    NEARBY_LOGS(INFO)
        << "Ignore ThroughputRecorder::start for Unknown Payload type";
    return;
  }

  MutexLock lock(&mutex_);
  start_timestamp_ = SystemClock::ElapsedRealtime();
  payload_type_ = payload_type;
  payload_direction_ = payload_direction;
  // Add packetLostAlarm later
}

bool ThroughputRecorder::Stop() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "Stop TP profiling for payload_id:" << payload_id_;
  if (payload_type_ == PayloadType::kUnknown) {
    NEARBY_LOGS(INFO) << "Ignore ThroughputRecorder::stop as it never start";
    return false;
  }
  {
    // Add packetLostAlarm stop process later
    absl::Time stop_timestamp = SystemClock::ElapsedRealtime();
    int64_t total_byte_size = 0;
    int medium_size = throughputs_.size();

    // The worse case is the socket/connect blocking the write request, never
    // got return when writing a frame out, it would get a very good data rate
    // for this case. e.g. use 60 seconds to send a file and failed, the counter
    // only get the duration as 30 seconds because the last write request
    // blocked.
    if (!success_) {
      if (!throughputs_.empty()) {
        for (auto& tp : throughputs_) {
          tp.second.SetLastTimestamp(stop_timestamp);
        }
      }
    }

    // calculate throughput by medium
    for (auto& tp : throughputs_) {
      tp.second.dump();
      total_byte_size += tp.second.GetTotalByteSize();
    }

    throughputs_.clear();

    int64_t total_millis =
        absl::ToInt64Milliseconds(stop_timestamp - start_timestamp_);
    throughput_kbps_ = CalculateThroughputKBps(total_byte_size, total_millis);
    int throughput_mbps = CalculateThroughputMBps(throughput_kbps_);

    // calculate overall throughput if there are multiple mediums
    if (medium_size > 1) {
      if (throughput_kbps_ != kDefaultThroughoutKbps) {
        std::string dump_content = absl::StrFormat(
            "%s %s data(%d bytes) %s, overall used %d milliseconds, "
            "throughput "
            "is %d MB/s (%d KB/s), File IO takes %d ms, %s takes %d "
            "ms, "
            "Socket IO takes %d ms",
            (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD)
                ? "Received"
                : "Sent",
            ToString(payload_type_), total_byte_size,
            success_ ? "SUCCEEDED" : "FAILED", total_millis, throughput_mbps,
            throughput_kbps_, file_io_time_,
            (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD)
                ? "Decryption"
                : "Encryption",
            encryption_time_, socket_io_time_);
        NEARBY_LOGS(INFO) << dump_content;
      }
    }
  }
  return true;
}

void ThroughputRecorder::MarkAsSuccess() {
  MutexLock lock(&mutex_);
  success_ = true;
}

int ThroughputRecorder::CalculateThroughputKBps(int64_t total_byte_size,
                                                int64_t total_millis) {
  if (total_millis > 0) {
    return (int)(total_byte_size * kSecInMs / kKbInBytes / total_millis);
  }
  return kDefaultThroughoutKbps;
}

int ThroughputRecorder::CalculateThroughputMBps(int throughputKBps) {
  return throughputKBps / kKbInBytes;
}

void ThroughputRecorder::Throughput::Add(int frame_size, int64_t file_io_time,
                                         int64_t encryption_time,
                                         int64_t socket_io_time) {
  total_byte_size_ += frame_size;
  // reset the last timestamp
  last_timestamp_ = SystemClock::ElapsedRealtime();
  file_io_time_ += file_io_time;
  encryption_time_ += encryption_time;
  socket_io_time_ += socket_io_time;
}

bool ThroughputRecorder::Throughput::dump() {
  int64_t total_millis =
      absl::ToInt64Milliseconds(last_timestamp_ - start_timestamp_);
  int throughput_kbps = CalculateThroughputKBps(total_byte_size_, total_millis);
  if (throughput_kbps == kDefaultThroughoutKbps) {
    return false;
  }
  int throughpu_mbps = CalculateThroughputMBps(throughput_kbps);
  int64_t other =
      total_millis - file_io_time_ - encryption_time_ - socket_io_time_;
  std::string dump_content = absl::StrFormat(
      "%s %s data(%ld bytes) via %s used %ld milliseconds, throughput is %d "
      "MB/s (%d KB/s), File IO takes %ld ms, %s takes %ld ms, "
      "Socket IO takes %ld ms, "
      "Other takes %ld ms",
      (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD) ? "Received"
                                                                 : "Sent",
      ToString(payload_type_), total_byte_size_,
      location::nearby::proto::connections::Medium_Name(medium_), total_millis,
      throughpu_mbps, throughput_kbps, file_io_time_,
      (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD) ? "Decryption"
                                                                 : "Encryption",
      encryption_time_, socket_io_time_, other);
  NEARBY_LOGS(INFO) << dump_content;
  return true;
}

ThroughputRecorder::Throughput& ThroughputRecorder::GetThroughput(
    Medium medium, int64_t duration_millis) {
  auto it = throughputs_.find(medium);
  if (it == throughputs_.end()) {
    auto throughput = new Throughput(
        medium,
        SystemClock::ElapsedRealtime() - absl::Milliseconds(duration_millis),
        payload_type_, payload_direction_);
    throughputs_.emplace(medium, std::move(*throughput));
    delete throughput;
    return throughputs_.find(medium)->second;
  }
  return it->second;
}

int ThroughputRecorder::GetThroughputsSize() {
  MutexLock lock(&mutex_);
  return throughputs_.size();
}

int ThroughputRecorder::GetThroughputKbps() { return throughput_kbps_; }

int64_t ThroughputRecorder::GetDurationMillis() { return duration_millis_; }

void ThroughputRecorder::OnFrameSent(Medium medium,
                                     PacketMetaData& packetMetaData) {
  MutexLock lock(&mutex_);
  if (payload_type_ == PayloadType::kUnknown) {
    NEARBY_LOGS(INFO) << "PayloadType is invalid, return";
    return;
  }

  duration_millis_ = packetMetaData.GetEncryptionTimeInMillis() +
                     packetMetaData.GetFileIoTimeInMillis() +
                     packetMetaData.GetSocketIoTimeInMillis();
  GetThroughput(medium, duration_millis_)
      .Add(packetMetaData.packet_size, packetMetaData.GetFileIoTimeInMillis(),
           packetMetaData.GetEncryptionTimeInMillis(),
           packetMetaData.GetSocketIoTimeInMillis());
  CalculateDurationTimes(packetMetaData);
}

void ThroughputRecorder::OnFrameReceived(Medium medium,
                                         PacketMetaData& packetMetaData) {
  MutexLock lock(&mutex_);
  if (payload_type_ == PayloadType::kUnknown) {
    NEARBY_LOGS(INFO) << "PayloadType is invalid, return";
    return;
  }

  // Add packetLostAlarm process later
  duration_millis_ = packetMetaData.GetEncryptionTimeInMillis() +
                     packetMetaData.GetFileIoTimeInMillis() +
                     packetMetaData.GetSocketIoTimeInMillis();
  GetThroughput(medium, duration_millis_)
      .Add(packetMetaData.packet_size, packetMetaData.GetFileIoTimeInMillis(),
           packetMetaData.GetEncryptionTimeInMillis(),
           packetMetaData.GetSocketIoTimeInMillis());
  CalculateDurationTimes(packetMetaData);
}

void ThroughputRecorder::CalculateDurationTimes(PacketMetaData packetMetaData) {
  encryption_time_ += packetMetaData.GetEncryptionTimeInMillis();
  socket_io_time_ += packetMetaData.GetSocketIoTimeInMillis();
  file_io_time_ += packetMetaData.GetFileIoTimeInMillis();
}

std::string ThroughputRecorder::ToString(PayloadType type) {
  switch (type) {
    case PayloadType::kBytes:
      return std::string("Bytes");
    case PayloadType::kStream:
      return std::string("Stream");
    case PayloadType::kFile:
      return std::string("File");
    case PayloadType::kUnknown:
      return std::string("Unknown");
  }
}

// Inplementation for ThroughputRecorderContainer

void ThroughputRecorderContainer::Shutdown() {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << __func__
                    << ".  Num of Instance:" << throughput_recorders_.size();
  for (auto& throughput_recorder : throughput_recorders_) {
    NEARBY_LOGS(INFO) << "Stop instance: " << throughput_recorder.second;
    throughput_recorder.second->Stop();
    delete throughput_recorder.second;
  }
  throughput_recorders_.clear();
}

ThroughputRecorder* ThroughputRecorderContainer::GetTPRecorder(
    const int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it == throughput_recorders_.end()) {
    auto instance = new ThroughputRecorder(payload_id);
    std::string direction =
        (payload_direction == PayloadDirection::INCOMING_PAYLOAD) ? "; Receive"
                                                                  : "; Send";
    NEARBY_LOGS(INFO) << "Add ThroughputRecorder instance : " << instance
                      << " for payload_id:" << payload_id << direction;
    throughput_recorders_.emplace(
        std::pair<int64_t, PayloadDirection>(payload_id, payload_direction),
        instance);
    return instance;
  }

  return it->second;
}

void ThroughputRecorderContainer::StopTPRecorder(
    const int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  std::string direction =
      (payload_direction == PayloadDirection::INCOMING_PAYLOAD) ? "; Receive"
                                                                : "; Send";
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    NEARBY_LOGS(INFO) << "Found and stop/delete ThroughputRecorder instance : "
                      << &(it->second) << " for payload_id:" << payload_id
                      << direction;
    it->second->Stop();
    delete it->second;
    throughput_recorders_.erase(
        std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
    return;
  }
  NEARBY_LOGS(INFO) << "No ThroughputRecorder found for :" << payload_id;
}

int ThroughputRecorderContainer::GetSize() {
  MutexLock lock(&mutex_);
  return throughput_recorders_.size();
}

}  // namespace analytics
}  // namespace nearby
