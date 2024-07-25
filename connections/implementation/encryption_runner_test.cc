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

#include "connections/implementation/encryption_runner.h"

#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "internal/platform/system_clock.h"
#include "proto/connections_enums.pb.h"
#include "third_party/ukey2/src/main/cpp/include/securegcm/ukey2_handshake.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::proto::connections::Medium;
constexpr size_t kChunkSize = 64 * 1024;

class FakeEndpointChannel : public EndpointChannel {
 public:
  FakeEndpointChannel(InputStream* in, OutputStream* out)
      : in_(in), out_(out) {}
  ExceptionOr<ByteArray> Read() override {
    read_timestamp_ = SystemClock::ElapsedRealtime();
    return in_ ? in_->Read(kChunkSize) : ExceptionOr<ByteArray>{Exception::kIo};
  }
  ExceptionOr<ByteArray> Read(PacketMetaData& packet_meta_data) override {
    read_timestamp_ = SystemClock::ElapsedRealtime();
    return in_ ? in_->Read(kChunkSize) : ExceptionOr<ByteArray>{Exception::kIo};
  }
  Exception Write(const ByteArray& data) override {
    write_timestamp_ = SystemClock::ElapsedRealtime();
    return out_ ? out_->Write(data) : Exception{Exception::kIo};
  }
  Exception Write(const ByteArray& data,
                  PacketMetaData& packet_meta_data) override {
    write_timestamp_ = SystemClock::ElapsedRealtime();
    return out_ ? out_->Write(data) : Exception{Exception::kIo};
  }
  void Close() override {
    if (in_) in_->Close();
    if (out_) out_->Close();
  }
  void Close(location::nearby::proto::connections::DisconnectionReason reason)
      override {
    Close();
  }
  void Close(
      location::nearby::proto::connections::DisconnectionReason reason,
      location::nearby::analytics::proto::ConnectionsLog::
          EstablishedConnection::SafeDisconnectionResult result) override {
    Close();
  }
  location::nearby::proto::connections::ConnectionTechnology GetTechnology()
      const override {
    return location::nearby::proto::connections::ConnectionTechnology::
        CONNECTION_TECHNOLOGY_BLE_GATT;
  }
  location::nearby::proto::connections::ConnectionBand GetBand()
      const override {
    return location::nearby::proto::connections::ConnectionBand::
        CONNECTION_BAND_CELLULAR_BAND_2G;
  }
  int GetFrequency() const override { return 0; }
  int GetTryCount() const override { return 0; }
  std::string GetType() const override { return "fake-channel-type"; }
  std::string GetServiceId() const override { return "fake-service-id"; }
  std::string GetName() const override { return "fake-channel"; }
  Medium GetMedium() const override { return Medium::BLE; }
  int GetMaxTransmitPacketSize() const override { return 512; }
  void EnableEncryption(std::shared_ptr<EncryptionContext> context) override {}
  void DisableEncryption() override {}
  bool IsEncrypted() override { return false; }
  ExceptionOr<ByteArray> TryDecrypt(const ByteArray& data) override {
    return Exception::kFailed;
  }

  bool IsPaused() const override { return false; }
  void Pause() override {}
  void Resume() override {}
  absl::Time GetLastReadTimestamp() const override { return read_timestamp_; }
  absl::Time GetLastWriteTimestamp() const override { return write_timestamp_; }
  void SetAnalyticsRecorder(analytics::AnalyticsRecorder* analytics_recorder,
                            const std::string& endpoint_id) override {}

 private:
  InputStream* in_ = nullptr;
  OutputStream* out_ = nullptr;
  absl::Time read_timestamp_ = absl::InfinitePast();
  absl::Time write_timestamp_ = absl::InfinitePast();
};

struct User {
  User(InputStream* reader, OutputStream* writer) : channel(reader, writer) {}

  FakeEndpointChannel channel;
  EncryptionRunner crypto;
  ClientProxy client;
};

struct Response {
  enum class Status {
    kUnknown = 0,
    kDone = 1,
    kFailed = 2,
  };

  CountDownLatch latch{2};
  Status server_status = Status::kUnknown;
  Status client_status = Status::kUnknown;
};

TEST(EncryptionRunnerTest, ConstructorDestructorWorks) { EncryptionRunner enc; }

TEST(EncryptionRunnerTest, ReadWrite) {
  auto from_a_to_b = CreatePipe();
  auto from_b_to_a = CreatePipe();
  User user_a(/*reader=*/from_b_to_a.first.get(),
              /*writer=*/from_a_to_b.second.get());
  User user_b(/*reader=*/from_a_to_b.first.get(),
              /*writer=*/from_b_to_a.second.get());
  Response response;

  user_a.crypto.StartServer(
      &user_a.client, "endpoint_id", &user_a.channel,
      {
          .on_success_cb =
              [&response](const std::string& endpoint_id,
                          std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                          const std::string& auth_token,
                          const ByteArray& raw_auth_token) {
                response.server_status = Response::Status::kDone;
                response.latch.CountDown();
              },
          .on_failure_cb =
              [&response](const std::string& endpoint_id,
                          EndpointChannel* channel) {
                response.server_status = Response::Status::kFailed;
                response.latch.CountDown();
              },
      });
  user_b.crypto.StartClient(
      &user_b.client, "endpoint_id", &user_b.channel,
      {
          .on_success_cb =
              [&response](const std::string& endpoint_id,
                          std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                          const std::string& auth_token,
                          const ByteArray& raw_auth_token) {
                response.client_status = Response::Status::kDone;
                response.latch.CountDown();
              },
          .on_failure_cb =
              [&response](const std::string& endpoint_id,
                          EndpointChannel* channel) {
                response.client_status = Response::Status::kFailed;
                response.latch.CountDown();
              },
      });
  EXPECT_TRUE(response.latch.Await(absl::Milliseconds(5000)).result());
  EXPECT_EQ(response.server_status, Response::Status::kDone);
  EXPECT_EQ(response.client_status, Response::Status::kDone);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
