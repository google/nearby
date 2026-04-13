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
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/packet_meta_data.h"
#include "connections/payload_type.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace analytics {

using Medium = ::location::nearby::proto::connections::Medium;
using ::nearby::connections::PayloadDirection;
using ::nearby::connections::PayloadType;

namespace {
constexpr int kDefaultThroughoutKbps = 0;
constexpr int kKbInBytes = 1024;
constexpr int kSecInMs = 1000;
}  // namespace



ThroughputRecorderContainer& ThroughputRecorderContainer::GetInstance() {
  static absl::NoDestructor<ThroughputRecorderContainer> instance;
  return *instance;
}

ThroughputRecorderContainer::ThroughputRecorder::ThroughputRecorder(
    int64_t payload_id)
    : payload_id_(payload_id) {}

void ThroughputRecorderContainer::ThroughputRecorder::Start(
    PayloadType payload_type, PayloadDirection payload_direction) {
  std::string direction =
      (payload_direction == PayloadDirection::INCOMING_PAYLOAD) ? "; Receive"
                                                                : "; Send";

  VLOG(1) << "Start TP profiling for payload_id:" << payload_id_ << direction;

  if (payload_type == PayloadType::kUnknown) {
    VLOG(1) << "Ignore ThroughputRecorder::start for Unknown Payload type";
    return;
  }

  MutexLock lock(&mutex_);
  start_timestamp_ = SystemClock::ElapsedRealtime();
  payload_type_ = payload_type;
  payload_direction_ = payload_direction;
}

bool ThroughputRecorderContainer::ThroughputRecorder::Stop() {
  MutexLock lock(&mutex_);
  VLOG(1) << "Stop TP profiling for payload_id:" << payload_id_;
  if (payload_type_ == PayloadType::kUnknown) {
    VLOG(1) << "Ignore ThroughputRecorder::stop as it never start";
    return false;
  }
  {
    absl::Time stop_timestamp = SystemClock::ElapsedRealtime();
    int64_t total_byte_size = 0;
    int medium_size = throughputs_.size();

    if (!success_) {
      if (!throughputs_.empty()) {
        for (auto& tp : throughputs_) {
          tp.second.SetLastTimestamp(stop_timestamp);
        }
      }
    }

    for (auto& tp : throughputs_) {
      tp.second.dump();
      total_byte_size += tp.second.GetTotalByteSize();
    }

    throughputs_.clear();

    int64_t total_millis =
        absl::ToInt64Milliseconds(stop_timestamp - start_timestamp_);
    throughput_kbps_ =
        ThroughputRecorderContainer::CalculateThroughputKBps(total_byte_size,
                                                            total_millis);
    int throughput_mbps =
        ThroughputRecorderContainer::CalculateThroughputMBps(throughput_kbps_);

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
            throughput_kbps_, (int)file_io_time_,
            (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD)
                ? "Decryption"
                : "Encryption",
            (int)encryption_time_, (int)socket_io_time_);
        LOG(INFO) << dump_content;
      }
    }
  }
  return true;
}

void ThroughputRecorderContainer::ThroughputRecorder::MarkAsSuccess() {
  MutexLock lock(&mutex_);
  success_ = true;
}



void ThroughputRecorderContainer::ThroughputRecorder::Throughput::Add(
    int frame_size, int64_t file_io_time, int64_t encryption_time,
    int64_t socket_io_time) {
  total_byte_size_ += frame_size;
  last_timestamp_ = SystemClock::ElapsedRealtime();
  file_io_time_ += file_io_time;
  encryption_time_ += encryption_time;
  socket_io_time_ += socket_io_time;
}

bool ThroughputRecorderContainer::ThroughputRecorder::Throughput::dump() {
  int64_t total_millis =
      absl::ToInt64Milliseconds(last_timestamp_ - start_timestamp_);
  int throughput_kbps =
      ThroughputRecorderContainer::CalculateThroughputKBps(total_byte_size_,
                                                           total_millis);
  if (throughput_kbps == kDefaultThroughoutKbps) {
    return false;
  }
  int throughpu_mbps =
      ThroughputRecorderContainer::CalculateThroughputMBps(throughput_kbps);
  int64_t other =
      total_millis - (int64_t)file_io_time_ - (int64_t)encryption_time_ -
      (int64_t)socket_io_time_;
  std::string dump_content = absl::StrFormat(
      "%s %s data(%d bytes) via %s used %d ms, throughput is %d "
      "MB/s (%d KB/s), File IO takes %d ms, %s takes %d ms, "
      "Socket IO takes %d ms, "
      "Other takes %d ms",
      (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD) ? "Received"
                                                                 : "Sent",
      ToString(payload_type_), total_byte_size_,
      location::nearby::proto::connections::Medium_Name(medium_),
      total_millis, throughpu_mbps, throughput_kbps, file_io_time_,
      (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD) ? "Decryption"
                                                                 : "Encryption",
      encryption_time_, socket_io_time_, other);
  LOG(INFO) << dump_content;
  return true;
}

ThroughputRecorderContainer::ThroughputRecorder::Throughput&
ThroughputRecorderContainer::ThroughputRecorder::GetThroughput(
    Medium medium, int64_t duration_millis) {
  auto it = throughputs_.find(medium);
  if (it == throughputs_.end()) {
    throughputs_.emplace(
        medium, Throughput(medium,
                           SystemClock::ElapsedRealtime() -
                               absl::Milliseconds(duration_millis),
                           payload_type_, payload_direction_));
    return throughputs_.find(medium)->second;
  }
  return it->second;
}

int ThroughputRecorderContainer::ThroughputRecorder::GetThroughputsSize()
    const {
  return throughputs_.size();
}

int ThroughputRecorderContainer::ThroughputRecorder::GetThroughputKbps()
    const {
  return throughput_kbps_;
}

int64_t ThroughputRecorderContainer::ThroughputRecorder::GetDurationMillis()
    const {
  return duration_millis_;
}

void ThroughputRecorderContainer::ThroughputRecorder::OnFrameSent(
    Medium medium, PacketMetaData& packetMetaData) {
  MutexLock lock(&mutex_);
  if (payload_type_ == PayloadType::kUnknown) {
    VLOG(1) << "PayloadType is invalid, return";
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

void ThroughputRecorderContainer::ThroughputRecorder::OnFrameReceived(
    Medium medium, PacketMetaData& packetMetaData) {
  MutexLock lock(&mutex_);
  if (payload_type_ == PayloadType::kUnknown) {
    VLOG(1) << "PayloadType is invalid, return";
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

void ThroughputRecorderContainer::ThroughputRecorder::CalculateDurationTimes(
    const PacketMetaData& packetMetaData) {
  encryption_time_ += packetMetaData.GetEncryptionTimeInMillis();
  socket_io_time_ += packetMetaData.GetSocketIoTimeInMillis();
  file_io_time_ += packetMetaData.GetFileIoTimeInMillis();
}

std::string ThroughputRecorderContainer::ThroughputRecorder::ToString(
    PayloadType type) {
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

// Implementation for ThroughputRecorderContainer

void ThroughputRecorderContainer::Start(int64_t payload_id,
                                        PayloadDirection payload_direction,
                                        PayloadType payload_type) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it == throughput_recorders_.end()) {
    auto instance = std::make_unique<ThroughputRecorder>(payload_id);
    instance->Start(payload_type, payload_direction);
    throughput_recorders_.emplace(
        std::pair<int64_t, PayloadDirection>(payload_id, payload_direction),
        std::move(instance));
  } else {
    it->second->Start(payload_type, payload_direction);
  }
}

void ThroughputRecorderContainer::OnFrameSent(
    int64_t payload_id, PayloadDirection payload_direction, Medium medium,
    PacketMetaData& packet_meta_data) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    it->second->OnFrameSent(medium, packet_meta_data);
  }
}

void ThroughputRecorderContainer::OnFrameReceived(
    int64_t payload_id, PayloadDirection payload_direction, Medium medium,
    PacketMetaData& packet_meta_data) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    it->second->OnFrameReceived(medium, packet_meta_data);
  }
}

void ThroughputRecorderContainer::MarkAsSuccess(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    it->second->MarkAsSuccess();
  }
}

int ThroughputRecorderContainer::StopTPRecorder(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    it->second->Stop();
    int throughput_kbps = it->second->GetThroughputKbps();
    throughput_recorders_.erase(it);
    return throughput_kbps;
  }
  return 0;
}

int ThroughputRecorderContainer::GetSize() {
  MutexLock lock(&mutex_);
  return throughput_recorders_.size();
}

void ThroughputRecorderContainer::ClearForTest() {
  MutexLock lock(&mutex_);
  throughput_recorders_.clear();
}

int ThroughputRecorderContainer::CalculateThroughputKBps(
    int64_t total_byte_size, int64_t total_millis) {
  if (total_millis > 0) {
    return (int)(total_byte_size * kSecInMs / kKbInBytes / total_millis);
  }
  return kDefaultThroughoutKbps;
}

int ThroughputRecorderContainer::CalculateThroughputMBps(int throughputKBps) {
  return throughputKBps / kKbInBytes;
}

int64_t ThroughputRecorderContainer::GetTotalByteSize(
    int64_t payload_id, PayloadDirection payload_direction, Medium medium) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughput(medium, 0).GetTotalByteSize();
  }
  return 0;
}

int ThroughputRecorderContainer::GetThroughputsSize(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughputsSize();
  }
  return 0;
}

int64_t ThroughputRecorderContainer::GetDurationMillis(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetDurationMillis();
  }
  return 0;
}

int ThroughputRecorderContainer::GetThroughputKbps(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughputKbps();
  }
  return 0;
}

bool ThroughputRecorderContainer::Dump(int64_t payload_id,
                                       PayloadDirection payload_direction,
                                       Medium medium) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughput(medium, 0).dump();
  }
  return false;
}

}  // namespace analytics
}  // namespace nearby
