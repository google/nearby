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

#include "core/internal/endpoint_channel.h"
#include "platform/api/atomic_boolean.h"
#include "platform/api/atomic_reference.h"
#include "platform/api/condition_variable.h"
#include "platform/api/input_stream.h"
#include "platform/api/lock.h"
#include "platform/api/output_stream.h"
#include "platform/api/system_clock.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"
#include "securegcm/d2d_connection_context_v1.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace connections {

class BaseEndpointChannel : public EndpointChannel {
 public:
  BaseEndpointChannel(absl::string_view channel_name, Ptr<InputStream> reader,
                      Ptr<OutputStream> writer);
  ~BaseEndpointChannel() override;

  ExceptionOr<ConstPtr<ByteArray> > read() override;

  Exception::Value write(ConstPtr<ByteArray> data) override;

  // Closes this EndpointChannel, without tracking the closure in analytics.
  void close() override;

  // Closes this EndpointChannel and records the closure with the given reason.
  void close(proto::connections::DisconnectionReason reason) override;

  // Returns a one-word type descriptor for the concrete EndpointChannel
  // implementation that can be used in log messages; eg: BLUETOOTH, BLE,
  // WIFI.
  string getType() override;

  // Returns the name of the EndpointChannel.
  string getName() override;

  // Enables encryption on the EndpointChannel.
  void enableEncryption(
      Ptr<securegcm::D2DConnectionContextV1> encryption_context) override;

  // True if the EndpointChannel is currently pausing all writes.
  bool isPaused() override;

  // Pauses all writes on this EndpointChannel until resume() is called.
  void pause() override;

  // Resumes any writes on this EndpointChannel that were suspended when pause()
  // was called.
  void resume() override;

  // Returns the timestamp (in elapsedRealtime) of the last read from this
  // endpoint, or -1 if no reads have occurred.
  std::int64_t getLastReadTimestamp() override;

 protected:
  virtual void closeImpl() = 0;

 private:
  // Used to sanity check that our frame sizes are reasonable.
  static constexpr std::int32_t kMaxAllowedReadBytes = 1048576;  // 1MB

  bool isEncryptionEnabled();
  void unblockPausedWriter();
  void blockUntilUnpaused();

  volatile std::int64_t last_read_timestamp_;

  const string channel_name_;

  ScopedPtr<Ptr<SystemClock> > system_clock_;

  // The reader and writer are synchronized independently since we can't have
  // writes waiting on reads that might potentially block forever.
  ScopedPtr<Ptr<Lock> > reader_lock_;
  // Not owned by this class, see the note in the destructor for a special
  // restriction on usage.
  Ptr<InputStream> reader_;

  ScopedPtr<Ptr<Lock> > writer_lock_;
  // Not owned by this class, see the note in the destructor for a special
  // restriction on usage.
  Ptr<OutputStream> writer_;

  // An encryptor/decryptor. May be null.
  ScopedPtr<Ptr<AtomicReference<Ptr<securegcm::D2DConnectionContextV1> > > >
      encryption_context_;

  ScopedPtr<Ptr<Lock> > is_paused_lock_;
  ScopedPtr<Ptr<ConditionVariable> > is_paused_condition_variable_;
  // If true, writes should block until this has been set to false.
  ScopedPtr<Ptr<AtomicBoolean> > is_paused_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BASE_ENDPOINT_CHANNEL_H_
