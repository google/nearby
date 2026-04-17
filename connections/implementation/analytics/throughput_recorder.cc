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

int64_t CalculateThroughputKBps(int64_t total_byte_size, int64_t total_millis) {
  if (total_millis > 0) {
    return total_byte_size * kSecInMs / kKbInBytes / total_millis;
  }
  return kDefaultThroughoutKbps;
}

int64_t CalculateThroughputMBps(int64_t throughputKBps) {
  return throughputKBps / kKbInBytes;
}

std::string ToString(PayloadType type) {
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
}  // namespace

ThroughputRecorderContainer& ThroughputRecorderContainer::GetInstance() {
  static absl::NoDestructor<ThroughputRecorderContainer> instance;
  return *instance;
}

ThroughputRecorderContainer::ThroughputRecorder::ThroughputRecorder(
    int64_t payload_id, PayloadDirection payload_direction,
    PayloadType payload_type)
    : payload_id_(payload_id),
      payload_direction_(payload_direction),
      payload_type_(payload_type) {
  LOG_IF(DFATAL, payload_type_ == PayloadType::kUnknown)
      << "Invalid payload type";
}

void ThroughputRecorderContainer::ThroughputRecorder::Start() {
  if (VLOG_IS_ON(1)) {
    std::string direction =
        (payload_direction_ == PayloadDirection::INCOMING_PAYLOAD) ? "; Receive"
                                                                   : "; Send";
    VLOG(1) << "Start TP profiling for payload_id:" << payload_id_ << direction;
  }

  start_timestamp_ = SystemClock::ElapsedRealtime();
}

bool ThroughputRecorderContainer::ThroughputRecorder::Stop() {
  VLOG(1) << "Stop TP profiling for payload_id:" << payload_id_;
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
      tp.second.dump(payload_direction_, payload_type_);
      total_byte_size += tp.second.GetTotalByteSize();
    }

    throughputs_.clear();

    int64_t total_millis =
        absl::ToInt64Milliseconds(stop_timestamp - start_timestamp_);
    throughput_kbps_ = CalculateThroughputKBps(total_byte_size, total_millis);
    int64_t throughput_mbps = CalculateThroughputMBps(throughput_kbps_);

    if (medium_size > 1) {
      if (throughput_kbps_ != kDefaultThroughoutKbps) {
        std::string dump_content = absl::StrFormat(
            "%s %s data(%lld bytes) %s, overall used %lld milliseconds, "
            "throughput "
            "is %lld MB/s (%lld KB/s), File IO takes %lld ms, %s takes %lld "
            "ms, "
            "Socket IO takes %lld ms",
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
        LOG(INFO) << dump_content;
      }
    }
  }
  return true;
}

void ThroughputRecorderContainer::ThroughputRecorder::MarkAsSuccess() {
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

bool ThroughputRecorderContainer::ThroughputRecorder::Throughput::dump(
    PayloadDirection payload_direction, PayloadType payload_type) {
  int64_t total_millis =
      absl::ToInt64Milliseconds(last_timestamp_ - start_timestamp_);
  int64_t throughput_kbps =
      CalculateThroughputKBps(total_byte_size_, total_millis);
  if (throughput_kbps == kDefaultThroughoutKbps) {
    return false;
  }
  int64_t throughput_mbps = CalculateThroughputMBps(throughput_kbps);
  int64_t other =
      total_millis - file_io_time_ - encryption_time_ - socket_io_time_;
  std::string dump_content = absl::StrFormat(
      "%s %s data(%lld bytes) via %s used %lld ms, throughput is %lld "
      "MB/s (%lld KB/s), File IO takes %lld ms, %s takes %lld ms, "
      "Socket IO takes %lld ms, "
      "Other takes %lld ms",
      (payload_direction == PayloadDirection::INCOMING_PAYLOAD) ? "Received"
                                                                : "Sent",
      ToString(payload_type), total_byte_size_,
      location::nearby::proto::connections::Medium_Name(medium_), total_millis,
      throughput_mbps, throughput_kbps, file_io_time_,
      (payload_direction == PayloadDirection::INCOMING_PAYLOAD) ? "Decryption"
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
        medium, Throughput(medium, SystemClock::ElapsedRealtime() -
                                       absl::Milliseconds(duration_millis)));
    return throughputs_.find(medium)->second;
  }
  return it->second;
}

int ThroughputRecorderContainer::ThroughputRecorder::GetThroughputsSize()
    const {
  return throughputs_.size();
}

int64_t ThroughputRecorderContainer::ThroughputRecorder::GetThroughputKbps()
    const {
  return throughput_kbps_;
}

int64_t ThroughputRecorderContainer::ThroughputRecorder::GetDurationMillis()
    const {
  return duration_millis_;
}

void ThroughputRecorderContainer::ThroughputRecorder::UpdateFrameData(
    Medium medium, PacketMetaData& packetMetaData) {
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

// Implementation for ThroughputRecorderContainer

void ThroughputRecorderContainer::Start(int64_t payload_id,
                                        PayloadDirection payload_direction,
                                        PayloadType payload_type) {
  if (payload_type == PayloadType::kUnknown) {
    return;
  }
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it == throughput_recorders_.end()) {
    auto instance = std::make_unique<ThroughputRecorder>(
        payload_id, payload_direction, payload_type);
    instance->Start();
    throughput_recorders_.emplace(
        std::pair<int64_t, PayloadDirection>(payload_id, payload_direction),
        std::move(instance));
  } else {
    it->second->Start();
  }
}

void ThroughputRecorderContainer::UpdateFrameData(
    int64_t payload_id, PayloadDirection payload_direction, Medium medium,
    PacketMetaData& packet_meta_data) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    it->second->UpdateFrameData(medium, packet_meta_data);
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

int64_t ThroughputRecorderContainer::StopTPRecorder(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    it->second->Stop();
    int64_t throughput_kbps = it->second->GetThroughputKbps();
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

int64_t ThroughputRecorderContainer::GetTotalByteSizeForTesting(
    int64_t payload_id, PayloadDirection payload_direction, Medium medium) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughput(medium, 0).GetTotalByteSize();
  }
  return 0;
}

int ThroughputRecorderContainer::GetThroughputsSizeForTesting(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughputsSize();
  }
  return 0;
}

int64_t ThroughputRecorderContainer::GetDurationMillisForTesting(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetDurationMillis();
  }
  return 0;
}

int64_t ThroughputRecorderContainer::GetThroughputKbpsForTesting(
    int64_t payload_id, PayloadDirection payload_direction) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughputKbps();
  }
  return 0;
}

bool ThroughputRecorderContainer::DumpForTesting(
    int64_t payload_id, PayloadDirection payload_direction, Medium medium) {
  MutexLock lock(&mutex_);
  auto it = throughput_recorders_.find(
      std::pair<int64_t, PayloadDirection>(payload_id, payload_direction));
  if (it != throughput_recorders_.end()) {
    return it->second->GetThroughput(medium, 0).dump(
        payload_direction, it->second->GetPayloadType());
  }
  return false;
}

}  // namespace analytics
}  // namespace nearby
