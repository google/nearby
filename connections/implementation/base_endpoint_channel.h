// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_BASE_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_BASE_ENDPOINT_CHANNEL_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {

using analytics::PacketMetaData;

class BaseEndpointChannel : public EndpointChannel {
 public:
  BaseEndpointChannel(const std::string& service_id,
                      const std::string& channel_name, InputStream* reader,
                      OutputStream* writer);
  BaseEndpointChannel(
      const std::string& service_id, const std::string& channel_name,
      InputStream* reader, OutputStream* writer,
      location::nearby::proto::connections::ConnectionTechnology,
      location::nearby::proto::connections::ConnectionBand band, int frequency,
      int try_count);
  ~BaseEndpointChannel() override = default;

  // EndpointChannel:
  ExceptionOr<ByteArray> Read() override;
  ExceptionOr<ByteArray> Read(PacketMetaData& packet_meta_data)
      ABSL_LOCKS_EXCLUDED(reader_mutex_, crypto_mutex_,
                          last_read_mutex_) override;
  Exception Write(const ByteArray& data) override;
  Exception Write(const ByteArray& data, PacketMetaData& packet_meta_data)
      ABSL_LOCKS_EXCLUDED(writer_mutex_, crypto_mutex_) override;
  void Close() ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;
  void Close(location::nearby::proto::connections::DisconnectionReason reason)
      override;
  std::string GetType() const override;
  std::string GetServiceId() const override;
  std::string GetName() const override;
  location::nearby::proto::connections::ConnectionTechnology GetTechnology()
      const override;
  location::nearby::proto::connections::ConnectionBand GetBand() const override;
  int GetFrequency() const override;
  int GetTryCount() const override;
  int GetMaxTransmitPacketSize() const override;
  void EnableEncryption(std::shared_ptr<EncryptionContext> context) override;
  void DisableEncryption() override;
  bool IsPaused() const ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;
  void Pause() ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;
  void Resume() ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;
  absl::Time GetLastReadTimestamp() const
      ABSL_LOCKS_EXCLUDED(last_read_mutex_) override;
  absl::Time GetLastWriteTimestamp() const
      ABSL_LOCKS_EXCLUDED(last_write_mutex_) override;
  void SetAnalyticsRecorder(analytics::AnalyticsRecorder* analytics_recorder,
                            const std::string& endpoint_id) override;

 protected:
  virtual void CloseImpl() = 0;

 private:
  // Used to sanity check that our frame sizes are reasonable.
  static constexpr std::int32_t kMaxAllowedReadBytes = 1048576;  // 1MB

  // The default maximum transmit unit/packet size.
  static constexpr int kDefaultMaxTransmitPacketSize = 65536;  // 64 KB

  bool IsEncryptionEnabledLocked() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(crypto_mutex_);
  void UnblockPausedWriter() ABSL_EXCLUSIVE_LOCKS_REQUIRED(is_paused_mutex_);
  void BlockUntilUnpaused() ABSL_EXCLUSIVE_LOCKS_REQUIRED(is_paused_mutex_);
  void CloseIo() ABSL_NO_THREAD_SAFETY_ANALYSIS;

  // We need a separate mutex to protect read timestamp, because if a read
  // blocks on IO, we don't want timestamp read access to block too.
  mutable Mutex last_read_mutex_;
  absl::Time last_read_timestamp_ ABSL_GUARDED_BY(last_read_mutex_) =
      absl::InfinitePast();

  // We need a separate mutex to protect write timestamp, because if a write
  // blocks on IO, we don't want timestamp write access to block too.
  mutable Mutex last_write_mutex_;
  absl::Time last_write_timestamp_ ABSL_GUARDED_BY(last_write_mutex_) =
      absl::InfinitePast();

  const std::string service_id_;
  const std::string channel_name_;

  // The reader and writer are synchronized independently since we can't have
  // writes waiting on reads that might potentially block forever.
  Mutex reader_mutex_;
  InputStream* reader_ ABSL_PT_GUARDED_BY(reader_mutex_);

  Mutex writer_mutex_;
  OutputStream* writer_ ABSL_PT_GUARDED_BY(writer_mutex_);

  // An encryptor/decryptor. May be null.
  mutable Mutex crypto_mutex_;
  std::shared_ptr<EncryptionContext> crypto_context_
      ABSL_GUARDED_BY(crypto_mutex_) ABSL_PT_GUARDED_BY(crypto_mutex_);

  mutable Mutex is_paused_mutex_;
  ConditionVariable is_paused_cond_{&is_paused_mutex_};
  // If true, writes should block until this has been set to false.
  bool is_paused_ ABSL_GUARDED_BY(is_paused_mutex_) = false;

  // The medium technology information of this endpoint channel.
  location::nearby::proto::connections::ConnectionTechnology technology_;
  location::nearby::proto::connections::ConnectionBand band_;
  int frequency_;
  int try_count_;

  analytics::AnalyticsRecorder* analytics_recorder_ = nullptr;
  std::string endpoint_id_ = "";
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BASE_ENDPOINT_CHANNEL_H_
