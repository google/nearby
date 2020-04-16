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

#ifndef CORE_INTERNAL_ENDPOINT_CHANNEL_H_
#define CORE_INTERNAL_ENDPOINT_CHANNEL_H_

#include <cstdint>

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "proto/connections_enums.pb.h"
#include "securegcm/d2d_connection_context_v1.h"

namespace location {
namespace nearby {
namespace connections {

class EndpointChannel {
 public:
  virtual ~EndpointChannel() {}

  virtual ExceptionOr<ConstPtr<ByteArray> >
  read() = 0;  // throws Exception::IO, Exception::INTERRUPTED

  virtual Exception::Value write(
      ConstPtr<ByteArray> data) = 0;  // throws Exception::IO

  // Closes this EndpointChannel, without tracking the closure in analytics.
  virtual void close() = 0;

  // Closes this EndpointChannel and records the closure with the given reason.
  virtual void close(proto::connections::DisconnectionReason reason) = 0;

  // Returns a one-word type descriptor for the concrete EndpointChannel
  // implementation that can be used in log messages; eg: BLUETOOTH, BLE, WIFI.
  virtual string getType() = 0;

  // Returns the name of the EndpointChannel.
  virtual string getName() = 0;

  // Returns the analytics enum representing the medium of this EndpointChannel.
  virtual proto::connections::Medium getMedium() = 0;

  // Enables encryption on the EndpointChannel.
  //
  // This method takes ownership of the passed-in 'connection_context'.
  virtual void enableEncryption(
      Ptr<securegcm::D2DConnectionContextV1> connection_context) = 0;

  // True if the EndpointChannel is currently pausing all writes.
  virtual bool isPaused() = 0;

  // Pauses all writes on this EndpointChannel until resume() is called.
  virtual void pause() = 0;

  // Resumes any writes on this EndpointChannel that were suspended when pause()
  // was called.
  virtual void resume() = 0;

  // Returns the timestamp of the last read from this endpoint, or -1 if no
  // reads have occurred.
  // TODO(tracyzhou): Clarify units of timestamp.
  virtual std::int64_t getLastReadTimestamp() = 0;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_ENDPOINT_CHANNEL_H_
