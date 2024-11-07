// Copyright 2024 Google LLC
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

#include "connections/implementation/mediums/multiplex/multiplex_socket.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/multiplex/multiplex_frames.h"
#include "connections/implementation/offline_frames.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/future.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/socket.h"
#include "proto/connections_enums.proto.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

constexpr absl::string_view SERVICE_ID_1 = "serviceId_1";
constexpr absl::string_view SERVICE_ID_2 = "serviceId_2";

using location::nearby::mediums::MultiplexFrame;
using location::nearby::mediums::MultiplexControlFrame;
using location::nearby::mediums::ConnectionResponseFrame;
using location::nearby::proto::connections::Medium;
using location::nearby::proto::connections::Medium_Name;

// A fake socket for testing.
class FakeSocket : public MediumSocket {
 public:
  explicit FakeSocket(Medium medium) : MediumSocket(medium) {
    pipe_1_ = CreatePipe();
    reader_1_ = std::move(pipe_1_.first);
    writer_1_ = std::move(pipe_1_.second);
    pipe_2_ = CreatePipe();
    reader_2_ = std::move(pipe_2_.first);
    writer_2_ = std::move(pipe_2_.second);
    NEARBY_LOGS(WARNING) << "Physical Socket Medium:"
                         << Medium_Name(GetMedium());
  };
  ~FakeSocket() override = default;

  FakeSocket(const FakeSocket&) = default;
  FakeSocket& operator=(const FakeSocket&) = default;

  /**
   * The constructor for a virtual socket which own the virtual {@link
   * OutputStream} and {@link InputStream}.
   */
  explicit FakeSocket(Medium medium, OutputStream* virtualOutputStream)
      : MediumSocket(medium), is_virtual_socket_(true) {
    pipe_1_ = CreatePipe();
    reader_1_ = std::move(pipe_1_.first);
    writer_1_ = std::move(pipe_1_.second);
    pipe_2_ = CreatePipe();
    reader_2_ = std::move(pipe_2_.first);
    writer_2_ = std::move(pipe_2_.second);
  }

  InputStream& GetInputStream() override { return *reader_1_; }
  OutputStream& GetOutputStream() override { return *writer_2_; }
  Exception Close() override {
    if (IsVirtualSocket()) {
      NEARBY_LOGS(INFO) << "Multiplex: Closing virtual socket: " << this;
      CloseLocal();
      return {Exception::kSuccess};
    }
    NEARBY_LOGS(INFO) << "Multiplex: Closing physical socket: " << this;
    reader_1_->Close();
    reader_2_->Close();
    writer_1_->Close();
    writer_2_->Close();
    return {Exception::kSuccess};
  }

  MediumSocket* CreateVirtualSocket(
      const std::string& salted_service_id_hash_key, OutputStream* outputstream,
      Medium medium,
      absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>*
          virtual_sockets_ptr) override {
    if (IsVirtualSocket()) {
      NEARBY_LOGS(WARNING)
          << "Creating the virtual socket on a virtual socket is not allowed.";
      return nullptr;
    }

    auto virtual_socket = std::make_shared<FakeSocket>(medium, outputstream);
    NEARBY_LOGS(WARNING) << "Created the virtual socket for Medium: "
                         << Medium_Name(virtual_socket->GetMedium());

    if (virtual_sockets_ptr_ == nullptr) {
      virtual_sockets_ptr_ = virtual_sockets_ptr;
    }

    (*virtual_sockets_ptr_)[salted_service_id_hash_key] = virtual_socket;
    NEARBY_LOGS(INFO) << "virtual_sockets_ size: "
                      << virtual_sockets_ptr_->size();
    return virtual_socket.get();
  }

  void FeedIncomingData(ByteArray data) override {
    bytes_read_future_.Set(data);
    NEARBY_LOGS(INFO) << "FeedIncomingData. Size of receive data: "
                      << data.size() << ", bytes content:" << std::string(data);
  }

  bool IsVirtualSocket() override { return is_virtual_socket_; }
  Future<ByteArray>& GetByteReadFuture() { return bytes_read_future_; }

  std::pair<std::unique_ptr<InputStream>, std::unique_ptr<OutputStream>>
      pipe_1_;
  std::unique_ptr<InputStream> reader_1_;
  std::unique_ptr<OutputStream> writer_1_;
  std::pair<std::unique_ptr<InputStream>, std::unique_ptr<OutputStream>>
      pipe_2_;
  std::unique_ptr<InputStream> reader_2_;
  std::unique_ptr<OutputStream> writer_2_;

 private:
  bool is_virtual_socket_ = false;
  Future<ByteArray> bytes_read_future_;
  absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>*
      virtual_sockets_ptr_ = nullptr;
};

TEST(MultiplexSocketTest, CreateSuccessAndReaderThreadStarted) {
  auto fake_socket_ptr =
      std::make_shared<FakeSocket>(Medium::BLUETOOTH);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_1),
                                                      Medium::BLUETOOTH);
  MultiplexSocket* multiplex_socket_incoming =
      MultiplexSocket::CreateIncomingSocket(
          fake_socket_ptr, std::string(SERVICE_ID_1), /*first_frame_len*/ 0);
  ASSERT_NE(multiplex_socket_incoming, nullptr);
  FakeSocket* virtual_socket =
      (FakeSocket*)multiplex_socket_incoming->GetVirtualSocket(
          std::string(SERVICE_ID_1));
  if (virtual_socket == nullptr) {
    NEARBY_LOGS(INFO) << "Virtual socket not found for " << SERVICE_ID_1;
    return;
  }

  SingleThreadExecutor executor;
  FakeSocket* socket = fake_socket_ptr.get();
  CountDownLatch latch(1);
  executor.Execute([socket, &latch]() {
    ByteArray connection_req_frame = parser::ForConnectionRequestConnections(
        {}, {
                .local_endpoint_id = "endpoint1",
                .local_endpoint_info = ByteArray("endpoint1 info"),
            });
    auto& writer = socket->writer_1_;
    NEARBY_LOGS(INFO) << "writer_1_ Write start";
    writer->Write(Base64Utils::IntToBytes(connection_req_frame.size()));
    writer->Write(connection_req_frame);
    writer->Flush();
    NEARBY_LOGS(INFO) << "writer_1_ Write end";
    latch.CountDown();
  });

  latch.Await(absl::Milliseconds(100));
  ExceptionOr<ByteArray> result = virtual_socket->GetByteReadFuture().Get();
  if (!result.ok()) {
    ADD_FAILURE() << "Read error: " << result.GetException().value;
  }
  ByteArray data = result.result();
  NEARBY_LOGS(INFO) << "Received " << data.size() << " bytes of data.";
  EXPECT_NE(data.size(), 0);
  absl::SleepFor(absl::Milliseconds(100));
  socket->reader_1_->Close();
  EXPECT_EQ(multiplex_socket_incoming->GetVirtualSocketCount(), 1);
  virtual_socket->Close();
  EXPECT_EQ(multiplex_socket_incoming->GetVirtualSocketCount(), 0);
}
TEST(MultiplexSocketTest, CreateFail_MediumNotSupport) {
  auto fake_socket_ptr =
      std::make_shared<FakeSocket>(Medium::WEB_RTC);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_1),
                                                      Medium::WEB_RTC);
  MultiplexSocket* multiplex_socket_incoming =
      MultiplexSocket::CreateIncomingSocket(
          fake_socket_ptr, std::string(SERVICE_ID_1), /*first_frame_len*/ 0);

  ASSERT_EQ(multiplex_socket_incoming, nullptr);
}

TEST(MultiplexSocketTest,
     EstablishVirtualSocket_ReturnNullWhenMultiplexSocketDisabled) {
  auto fake_socket_ptr = std::make_shared<FakeSocket>(Medium::BLE);

  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_1),
                                                      Medium::BLE);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_2),
                                                      Medium::BLE);
  MultiplexSocket* multiplex_socket = MultiplexSocket::CreateOutgoingSocket(
      fake_socket_ptr, std::string(SERVICE_ID_1));
  ASSERT_NE(multiplex_socket, nullptr);

  MediumSocket* socket =
      multiplex_socket->EstablishVirtualSocket(std::string(SERVICE_ID_2));
  EXPECT_EQ(socket, nullptr);
  absl::SleepFor(absl::Milliseconds(100));
  FakeSocket* virtual_socket =
      (FakeSocket*)multiplex_socket->GetVirtualSocket(
          std::string(SERVICE_ID_1));
  if (virtual_socket == nullptr) {
    NEARBY_LOGS(INFO) << "Virtual socket not found for " << SERVICE_ID_1;
    return;
  }
  fake_socket_ptr->reader_1_->Close();
  EXPECT_EQ(multiplex_socket->GetVirtualSocketCount(), 1);
  virtual_socket->Close();
  EXPECT_EQ(multiplex_socket->GetVirtualSocketCount(), 0);
}

TEST(MultiplexSocketTest,
     EstablishVirtualSocket_TimeoutBecauseNoConnectionResponse) {
  auto fake_socket_ptr = std::make_shared<FakeSocket>(Medium::WIFI_LAN);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_1),
                                                      Medium::WIFI_LAN);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_2),
                                                      Medium::WIFI_LAN);
  MultiplexSocket* multiplex_socket = MultiplexSocket::CreateOutgoingSocket(
      fake_socket_ptr, std::string(SERVICE_ID_1));
  ASSERT_NE(multiplex_socket, nullptr);
  multiplex_socket->Enable();
  FakeSocket* virtual_socket = (FakeSocket*)multiplex_socket->GetVirtualSocket(
      std::string(SERVICE_ID_1));
  if (virtual_socket == nullptr) {
    NEARBY_LOGS(INFO) << "Virtual socket not found for " << SERVICE_ID_1;
    return;
  }

  SingleThreadExecutor executor;
  CountDownLatch latch(1);
  executor.Execute([&multiplex_socket, &latch]() {
    NEARBY_LOGS(INFO) << "EstablishVirtualSocket";
    MediumSocket* socket =
        multiplex_socket->EstablishVirtualSocket(std::string(SERVICE_ID_2));
    NEARBY_LOGS(INFO) << "EstablishVirtualSocket finished";
    EXPECT_EQ(socket, nullptr);
    latch.CountDown();
  });
  latch.Await(absl::Milliseconds(3000));

  auto reader = fake_socket_ptr->reader_2_.get();
  NEARBY_LOGS(INFO) << "reader_2_ Read start";
  ExceptionOr<std::int32_t> read_int = Base64Utils::ReadInt(reader);
  if (!read_int.ok()) {
    ADD_FAILURE() << "Failed to read. Exception:"
                         << read_int.exception();
  }
  auto length = read_int.result();
  NEARBY_LOGS(INFO) << " length:" << length;
  EXPECT_GT(length, 0);
  EXPECT_EQ(multiplex_socket->GetVirtualSocket(std::string(SERVICE_ID_2)),
            nullptr);

  absl::SleepFor(absl::Milliseconds(100));
  fake_socket_ptr->reader_1_->Close();
  EXPECT_EQ(multiplex_socket->GetVirtualSocketCount(), 1);
  virtual_socket->Close();
  EXPECT_EQ(multiplex_socket->GetVirtualSocketCount(), 0);
}

TEST(MultiplexSocketTest,
     EstablishVirtualSocket_RemoteAccepted) {
  auto fake_socket_ptr = std::make_shared<FakeSocket>(Medium::BLUETOOTH);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_1),
                                                      Medium::BLUETOOTH);
  MultiplexSocket::StopListeningForIncomingConnection(std::string(SERVICE_ID_2),
                                                      Medium::BLUETOOTH);

  MultiplexSocket* multiplex_socket = MultiplexSocket::CreateOutgoingSocket(
      fake_socket_ptr, std::string(SERVICE_ID_1));
  ASSERT_NE(multiplex_socket, nullptr);
  multiplex_socket->Enable();

  SingleThreadExecutor executor;
  executor.Execute([&multiplex_socket]() {
    NEARBY_LOGS(INFO) << "EstablishVirtualSocket";
    MediumSocket* socket =
        multiplex_socket->EstablishVirtualSocket(std::string(SERVICE_ID_2));
    EXPECT_NE(socket, nullptr);
  });

  auto reader = fake_socket_ptr->reader_2_.get();
  NEARBY_LOGS(INFO) << "reader_2_ Waiting for CONNECTION_REQUEST frame.";
  ExceptionOr<std::int32_t> read_int = Base64Utils::ReadInt(reader);
  if (!read_int.ok()) {
    ADD_FAILURE() << "Failed to read length.Exception:" << read_int.exception();
  }
  auto length = read_int.result();
  if (length < 0 ||
      length >
          FeatureFlags::GetInstance().GetFlags().connection_max_frame_length) {
    ADD_FAILURE() << "Invalid length:" << length;
  }

  auto bytes = reader->ReadExactly(length);
  if (!bytes.ok()) {
    ADD_FAILURE() << "Failed to read frame. Exception:" << bytes.exception();
  }
  length = read_int.result();
  if (length < 0 ||
      length >
          FeatureFlags::GetInstance().GetFlags().connection_max_frame_length) {
    ADD_FAILURE() << "Invalid frame length:" << length;
  }

  ExceptionOr<MultiplexFrame> frame_exc =
      multiplex::FromBytes(bytes.result());
  if (!frame_exc.ok()) {
    ADD_FAILURE() << "Failed to parse MultiplexFrame. Exception:"
                  << frame_exc.exception();
  }
  auto frame = frame_exc.result();
  auto salted_service_id_hash =
      ByteArray{std::move(frame.header().salted_service_id_hash())};
  auto service_id_hash_salt = frame.header().has_service_id_hash_salt()
                                  ? frame.header().service_id_hash_salt()
                                  : "";
  ASSERT_EQ(frame.frame_type(), MultiplexFrame::CONTROL_FRAME);
  auto control_frame = frame.control_frame();
  ASSERT_EQ(control_frame.control_frame_type(),
            MultiplexControlFrame::CONNECTION_REQUEST);
  NEARBY_LOGS(INFO) << "Recieved MultiplexControlFrame::CONNECTION_REQUEST "
                       "frame, now send CONNECTION_RESPONSE frame.";

  ByteArray connection_response_frame =
      ForConnectionResponse(salted_service_id_hash, service_id_hash_salt,
                            ConnectionResponseFrame::CONNECTION_ACCEPTED);
  auto& writer = fake_socket_ptr->writer_1_;
  NEARBY_LOGS(INFO) << "writer_1_ Write start";
  writer->Write(Base64Utils::IntToBytes(connection_response_frame.size()));
  writer->Write(connection_response_frame);
  writer->Flush();
  NEARBY_LOGS(INFO) << "writer_1_ Write end";
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_NE(multiplex_socket->GetVirtualSocket(std::string(SERVICE_ID_2)),
            nullptr);

  fake_socket_ptr->reader_1_->Close();
  EXPECT_EQ(multiplex_socket->GetVirtualSocketCount(), 2);
  multiplex_socket->ShutdownAll();
  EXPECT_EQ(multiplex_socket->GetVirtualSocketCount(), 0);
}

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
