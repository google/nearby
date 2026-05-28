// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MOCK_ENDPOINT_CHANNEL_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MOCK_ENDPOINT_CHANNEL_H_

#include <cstdint>
#include <memory>
#include <string>
#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby::connections {

class MockEndpointChannel : public EndpointChannel {
 public:
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (), (override));
  MOCK_METHOD(Exception, Write, (absl::string_view data),
              (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(
      void, Close,
      (location::nearby::proto::connections::DisconnectionReason reason),
      (override));
  MOCK_METHOD(void, Close,
              (location::nearby::proto::connections::DisconnectionReason reason,
               nearby::analytics::SafeDisconnectionResult result),
              (override));
  MOCK_METHOD(bool, IsClosed, (), (const, override));
  MOCK_METHOD(std::string, GetType, (), (const, override));
  MOCK_METHOD(std::string, GetServiceId, (), (const, override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(location::nearby::proto::connections::Medium, GetMedium, (),
              (const, override));
  MOCK_METHOD(location::nearby::proto::connections::ConnectionTechnology,
              GetTechnology, (), (const, override));
  MOCK_METHOD(location::nearby::proto::connections::ConnectionBand, GetBand, (),
              (const, override));
  MOCK_METHOD(int, GetFrequency, (), (const, override));
  MOCK_METHOD(int, GetTryCount, (), (const, override));
  MOCK_METHOD(int, GetMaxTransmitPacketSize, (), (const, override));
  MOCK_METHOD(void, EnableEncryption, (std::shared_ptr<EncryptionContext>),
              (override));
  MOCK_METHOD(void, DisableEncryption, (), (override));
  MOCK_METHOD(bool, IsEncrypted, (), (override));
  MOCK_METHOD(ExceptionOr<ByteArray>, TryDecrypt, (const ByteArray& data),
              (override));
  MOCK_METHOD(bool, IsPaused, (), (const, override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(absl::Time, GetLastReadTimestamp, (), (const, override));
  MOCK_METHOD(absl::Time, GetLastWriteTimestamp, (), (const, override));
  MOCK_METHOD(uint32_t, GetNextKeepAliveSeqNo, (), (const, override));
  MOCK_METHOD(void, SetAnalyticsRecorder,
              (analytics::AnalyticsRecorder*, const std::string&), (override));
};

}  // namespace nearby::connections

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_MOCK_ENDPOINT_CHANNEL_H_
