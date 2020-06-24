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

#include "core_v2/internal/encryption_runner.h"

#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/pipe.h"
#include "platform_v2/public/system_clock.h"
#include "proto/connections_enums.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using ::location::nearby::proto::connections::Medium;

class FakeEndpointChannel : public EndpointChannel {
 public:
  FakeEndpointChannel(InputStream* in, OutputStream* out)
      : in_(in), out_(out) {}
  ExceptionOr<ByteArray> Read() override {
    read_timestamp_ = SystemClock::ElapsedRealtime();
    return in_ ? in_->Read(Pipe::kChunkSize)
               : ExceptionOr<ByteArray>{Exception::kIo};
  }
  Exception Write(const ByteArray& data) override {
    return out_ ? out_->Write(data) : Exception{Exception::kIo};
  }
  void Close() override {
    if (in_) in_->Close();
    if (out_) out_->Close();
  }
  void Close(proto::connections::DisconnectionReason reason) override {
    Close();
  }
  std::string GetType() const override { return "fake-channel-type"; }
  std::string GetName() const override { return "fake-channel"; }
  Medium GetMedium() const override { return Medium::BLE; }
  void EnableEncryption(std::shared_ptr<EncryptionContext> context) override {}
  bool IsPaused() const override { return false; }
  void Pause() override {}
  void Resume() override {}
  absl::Time GetLastReadTimestamp() const override { return read_timestamp_; }

 private:
  InputStream* in_ = nullptr;
  OutputStream* out_ = nullptr;
  absl::Time read_timestamp_ = absl::InfinitePast();
};

struct User {
  User(Pipe* reader, Pipe* writer)
      : channel(&reader->GetInputStream(), &writer->GetOutputStream()) {}

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
  Pipe from_a_to_b;
  Pipe from_b_to_a;
  User user_a(/*reader=*/&from_b_to_a, /*writer=*/&from_a_to_b);
  User user_b(/*reader=*/&from_a_to_b, /*writer=*/&from_b_to_a);
  Response response;

  user_a.crypto.StartServer(
      &user_a.client, "endpoint_id", &user_a.channel,
      {
          .on_success_cb =
              [&response](const string& endpoint_id,
                          std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                          const string& auth_token,
                          const ByteArray& raw_auth_token) {
                response.server_status = Response::Status::kDone;
                response.latch.CountDown();
              },
          .on_failure_cb =
              [&response](const string& endpoint_id, EndpointChannel* channel) {
                response.server_status = Response::Status::kFailed;
                response.latch.CountDown();
              },
      });
  user_b.crypto.StartClient(
      &user_b.client, "endpoint_id", &user_b.channel,
      {
          .on_success_cb =
              [&response](const string& endpoint_id,
                          std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                          const string& auth_token,
                          const ByteArray& raw_auth_token) {
                response.client_status = Response::Status::kDone;
                response.latch.CountDown();
              },
          .on_failure_cb =
              [&response](const string& endpoint_id, EndpointChannel* channel) {
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
}  // namespace location
