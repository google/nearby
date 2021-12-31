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

#include "securegcm/d2d_connection_context_v1.h"
#include "absl/base/thread_annotations.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/atomic_reference.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/mutex.h"
#include "internal/platform/system_clock.h"
#include "internal/analytics/analytics_recorder.h"

namespace location {
namespace nearby {
namespace connections {

class BaseEndpointChannel : public EndpointChannel {
 public:
  BaseEndpointChannel(const std::string& channel_name, InputStream* reader,
                      OutputStream* writer);
  BaseEndpointChannel(const std::string& channel_name, InputStream* reader,
                      OutputStream* writer,
                      proto::connections::ConnectionTechnology,
                      proto::connections::ConnectionBand band, int frequency,
                      int try_count);
  ~BaseEndpointChannel() override = default;

  ExceptionOr<ByteArray> Read()
      ABSL_LOCKS_EXCLUDED(reader_mutex_, crypto_mutex_,
                          last_read_mutex_) override;

  Exception Write(const ByteArray& data)
      ABSL_LOCKS_EXCLUDED(writer_mutex_, crypto_mutex_) override;

  // Closes this EndpointChannel, without tracking the closure in analytics.
  void Close() ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;

  // Closes this EndpointChannel and records the closure with the given reason.
  void Close(proto::connections::DisconnectionReason reason) override;

  // Returns a one-word type descriptor for the concrete EndpointChannel
  // implementation that can be used in log messages; eg: BLUETOOTH, BLE,
  // WIFI.
  std::string GetType() const override;

  // Returns the name of the EndpointChannel.
  std::string GetName() const override;

  // Returns the maximum supported transmit packet size(MTU) for the underlying
  // transport.
  int GetMaxTransmitPacketSize() const override;

  // Enables encryption on the EndpointChannel.
  // Should be called after connection is accepted by both parties, and
  // before entering data phase, where Payloads may be exchanged.
  void EnableEncryption(std::shared_ptr<EncryptionContext> context) override;

  // Disables encryption on the EndpointChannel.
  void DisableEncryption() override;

  // True if the EndpointChannel is currently pausing all writes.
  bool IsPaused() const ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;

  // Pauses all writes on this EndpointChannel until resume() is called.
  void Pause() ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;

  // Resumes any writes on this EndpointChannel that were suspended when pause()
  // was called.
  void Resume() ABSL_LOCKS_EXCLUDED(is_paused_mutex_) override;

  // Returns the timestamp (returned by ElapsedRealtime) of the last read from
  // this endpoint, or -1 if no reads have occurred.
  absl::Time GetLastReadTimestamp() const
      ABSL_LOCKS_EXCLUDED(last_read_mutex_) override;

  // Returns the timestamp (returned by ElapsedRealtime) of the last write to
  // this endpoint, or -1 if no writes have occurred.
  absl::Time GetLastWriteTimestamp() const
      ABSL_LOCKS_EXCLUDED(last_write_mutex_) override;

  // Returns the used technology of this EndpointChannel.
  proto::connections::ConnectionTechnology GetTechnology() const override;

  // Returns the used wifi band of this EndpointChannel.
  proto::connections::ConnectionBand GetBand() const override;

  // Returns the used wifi frequency of this EndpointChannel.
  int GetFrequency() const override;

  // Returns the try count of this EndpointChannel.
  int GetTryCount() const override;

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
  proto::connections::ConnectionTechnology technology_;
  proto::connections::ConnectionBand band_;
  int frequency_;
  int try_count_;

  analytics::AnalyticsRecorder* analytics_recorder_ = nullptr;
  std::string endpoint_id_ = "";
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BASE_ENDPOINT_CHANNEL_H_
