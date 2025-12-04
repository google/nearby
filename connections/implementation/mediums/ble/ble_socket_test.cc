// Copyright 2025 Google LLC
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

#include "connections/implementation/mediums/ble/ble_socket.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble/ble_l2cap_packet.h"
#include "connections/implementation/mediums/ble/ble_packet.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/byte_utils.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "proto/connections_enums.proto.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

using ::location::nearby::proto::connections::Medium;

constexpr absl::string_view kServiceIdHash{"\x0a\x0b\x0c"};

class FakeInputStream : public InputStream {
 public:
  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    if (exception_on_read_) {
      return ExceptionOr<ByteArray>(Exception::kIo);
    }
    if (buffer_.empty()) {
      return ExceptionOr<ByteArray>(Exception::kIo);
    }
    size_t read_size = std::min(static_cast<size_t>(size), buffer_.size());
    ByteArray result{buffer_.data(), read_size};
    buffer_.erase(0, read_size);
    return ExceptionOr<ByteArray>{result};
  }
  Exception Close() override { return {Exception::kSuccess}; }

  void Append(const ByteArray& data) {
    absl::StrAppend(&buffer_, std::string(data));
  }

  void SetExceptionOnRead(bool exception_on_read) {
    exception_on_read_ = exception_on_read;
  }

 private:
  std::string buffer_;
  bool exception_on_read_ = false;
};

class FakeOutputStream : public OutputStream {
 public:
  Exception Write(const ByteArray& data) override {
    if (exception_on_write_) {
      return {Exception::kIo};
    }
    absl::StrAppend(&buffer_, std::string(data));
    return {Exception::kSuccess};
  }
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override { return {Exception::kSuccess}; }
  std::string GetPayload() { return buffer_; }
  void Clear() { buffer_.clear(); }
  void SetExceptionOnWrite(bool exception_on_write) {
    exception_on_write_ = exception_on_write;
  }

 private:
  std::string buffer_;
  bool exception_on_write_ = false;
};

class FakeBleSocketImpl : public api::ble::BleSocket {
 public:
  FakeBleSocketImpl(InputStream& input_stream, OutputStream& output_stream)
      : input_stream_(input_stream), output_stream_(output_stream) {}

  api::ble::BlePeripheral::UniqueId GetRemotePeripheralId() override {
    return {};
  }
  InputStream& GetInputStream() override { return input_stream_; }
  OutputStream& GetOutputStream() override { return output_stream_; }
  Exception Close() override {
    closed_ = true;
    return close_exception_;
  }

  bool IsClosed() const { return closed_; }
  void SetCloseException(Exception exception) { close_exception_ = exception; }

 private:
  InputStream& input_stream_;
  OutputStream& output_stream_;
  bool closed_ = false;
  Exception close_exception_ = {Exception::kSuccess};
};

class FakeBleL2capSocketImpl : public api::ble::BleL2capSocket {
 public:
  FakeBleL2capSocketImpl(InputStream& input_stream, OutputStream& output_stream)
      : input_stream_(input_stream), output_stream_(output_stream) {}

  api::ble::BlePeripheral::UniqueId GetRemotePeripheralId() override {
    return {};
  }
  InputStream& GetInputStream() override { return input_stream_; }
  OutputStream& GetOutputStream() override { return output_stream_; }
  Exception Close() override {
    closed_ = true;
    return close_exception_;
  }

  bool IsClosed() const { return closed_; }
  void SetCloseException(Exception exception) { close_exception_ = exception; }

 private:
  InputStream& input_stream_;
  OutputStream& output_stream_;
  bool closed_ = false;
  Exception close_exception_ = {Exception::kSuccess};
};

class BleSocketBleMediumTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nearby::BlePeripheral peripheral;

    auto fake_socket_impl = std::make_unique<FakeBleSocketImpl>(
        fake_input_stream_, fake_output_stream_);
    fake_socket_impl_ = fake_socket_impl.get();

    nearby::BleSocket platform_socket(peripheral, std::move(fake_socket_impl));

    socket_ = BleSocket::CreateWithBleSocket(
        std::move(platform_socket), ByteArray(std::string(kServiceIdHash)));
    ASSERT_NE(socket_, nullptr);
  }

  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
  FakeBleSocketImpl* fake_socket_impl_;
  std::unique_ptr<BleSocket> socket_;
};

TEST_F(BleSocketBleMediumTest, CloseSucceeds) {
  EXPECT_TRUE(socket_->Close().Ok());
}

TEST_F(BleSocketBleMediumTest, CloseFails_PropagatesException) {
  fake_socket_impl_->SetCloseException({Exception::kIo});

  Exception result = socket_->Close();

  EXPECT_EQ(result.value, Exception::kIo);
}

TEST_F(BleSocketBleMediumTest, GetInputStreamReturnsSameStreamInstance) {
  InputStream& stream1 = socket_->GetInputStream();
  InputStream& stream2 = socket_->GetInputStream();

  EXPECT_EQ(&stream1, &stream2);
}

TEST_F(BleSocketBleMediumTest, GetOutputStreamReturnsSameStreamInstance) {
  OutputStream& stream1 = socket_->GetOutputStream();
  OutputStream& stream2 = socket_->GetOutputStream();

  EXPECT_EQ(&stream1, &stream2);
}

TEST_F(BleSocketBleMediumTest, GetRemotePeripheralReturnsSameInstance) {
  nearby::BlePeripheral& peripheral1 = socket_->GetRemotePeripheral();
  nearby::BlePeripheral& peripheral2 = socket_->GetRemotePeripheral();

  EXPECT_EQ(&peripheral1, &peripheral2);
}

TEST_F(BleSocketBleMediumTest, DispatchPacketWithCorrectServiceIdHash) {
  fake_input_stream_.Append(ByteArray(std::string(kServiceIdHash)));
  auto result = socket_->DispatchPacket();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray(std::string(kServiceIdHash)));
}

TEST_F(BleSocketBleMediumTest, DispatchPacketWithWrongServiceIdHash) {
  fake_input_stream_.Append(ByteArray("\x01\x02\x03"));
  auto result = socket_->DispatchPacket();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kFailed);
}

TEST_F(BleSocketBleMediumTest, DispatchPacketWithControlPacket) {
  auto control_packet_status = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(control_packet_status.ok());
  BlePacket control_packet = control_packet_status.value();
  fake_input_stream_.Append(ByteArray(control_packet));
  fake_input_stream_.Append(ByteArray(std::string(kServiceIdHash)));

  auto result = socket_->DispatchPacket();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray(std::string(kServiceIdHash)));
}

TEST_F(BleSocketBleMediumTest, DispatchPacketWithReadError) {
  fake_input_stream_.SetExceptionOnRead(true);
  auto result = socket_->DispatchPacket();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kIo);
}

TEST_F(BleSocketBleMediumTest, ReadPayloadLengthSuccess) {
  constexpr int kPayloadLength = 12345;
  fake_input_stream_.Append(ByteArray(byte_utils::IntToBytes(kPayloadLength)));
  auto result = socket_->ReadPayloadLength();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), kPayloadLength);
}

TEST_F(BleSocketBleMediumTest, ReadPayloadLengthReadError) {
  fake_input_stream_.SetExceptionOnRead(true);
  auto result = socket_->ReadPayloadLength();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kIo);
}

TEST_F(BleSocketBleMediumTest, ReadPayloadLengthAfterClose) {
  socket_->Close();
  auto result = socket_->ReadPayloadLength();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kIo);
}

TEST_F(BleSocketBleMediumTest, WritePayloadLengthSuccess) {
  constexpr int kPayloadLength = 12345;
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleSocketBleMediumTest, WritePayloadLengthAfterClose) {
  constexpr int kPayloadLength = 12345;
  socket_->Close();
  EXPECT_FALSE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleSocketBleMediumTest, WritePayloadLengthFailsIfCalledTwice) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kRefactorBleL2cap,
      true);
  constexpr int kPayloadLength = 12345;
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
  EXPECT_FALSE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleSocketBleMediumTest, WritePayloadLengthSucceedsIfCalledAfterWrite) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kRefactorBleL2cap,
      true);
  constexpr int kPayloadLength = 12345;
  ByteArray payload("payload");
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
  EXPECT_TRUE(socket_->GetOutputStream().Write(payload).Ok());
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleSocketBleMediumTest, IsValid) {
  EXPECT_TRUE(socket_->IsValid());
  EXPECT_TRUE(socket_->Close().Ok());
  EXPECT_TRUE(socket_->IsValid());
}

TEST_F(BleSocketBleMediumTest, SendIntroductionSuccess) {
  EXPECT_TRUE(socket_->SendIntroduction().Ok());
  auto intro_packet = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(intro_packet.ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(ByteArray(*intro_packet)));
}

TEST_F(BleSocketBleMediumTest, SendIntroductionAfterClose) {
  socket_->Close();
  EXPECT_FALSE(socket_->SendIntroduction().Ok());
}

TEST_F(BleSocketBleMediumTest, SendIntroductionWriteFails) {
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->SendIntroduction().Ok());
}

TEST_F(BleSocketBleMediumTest, GetMedium) {
  EXPECT_EQ(socket_->GetMedium(), Medium::BLE);
}

TEST_F(BleSocketBleMediumTest, SendDisconnectionSuccess) {
  EXPECT_TRUE(socket_->SendDisconnection().Ok());
  auto disconnection_packet = BlePacket::CreateControlDisconnectionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(disconnection_packet.ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(ByteArray(*disconnection_packet)));
}

TEST_F(BleSocketBleMediumTest, SendDisconnectionAfterClose) {
  socket_->Close();
  EXPECT_FALSE(socket_->SendDisconnection().Ok());
}

TEST_F(BleSocketBleMediumTest, SendDisconnectionWriteFails) {
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->SendDisconnection().Ok());
}

TEST_F(BleSocketBleMediumTest, SendPacketAcknowledgementSuccess) {
  constexpr int kReceivedSize = 100;
  EXPECT_TRUE(socket_->SendPacketAcknowledgement(kReceivedSize).Ok());
  auto ack_packet = BlePacket::CreateControlPacketAcknowledgementPacket(
      ByteArray(std::string(kServiceIdHash)), kReceivedSize);
  ASSERT_TRUE(ack_packet.ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(ByteArray(*ack_packet)));
}

TEST_F(BleSocketBleMediumTest, SendPacketAcknowledgementAfterClose) {
  constexpr int kReceivedSize = 100;
  socket_->Close();
  EXPECT_FALSE(socket_->SendPacketAcknowledgement(kReceivedSize).Ok());
}

TEST_F(BleSocketBleMediumTest, SendPacketAcknowledgementWriteFails) {
  constexpr int kReceivedSize = 100;
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->SendPacketAcknowledgement(kReceivedSize).Ok());
}

class BleL2capSocketBleMediumTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nearby::BlePeripheral peripheral;
    auto fake_l2cap_socket_impl = std::make_unique<FakeBleL2capSocketImpl>(
        fake_input_stream_, fake_output_stream_);

    fake_l2cap_socket_impl_ = fake_l2cap_socket_impl.get();

    nearby::BleL2capSocket platform_l2cap_socket(
        peripheral, std::move(fake_l2cap_socket_impl));

    socket_ = BleSocket::CreateWithL2capSocket(
        std::move(platform_l2cap_socket),
        ByteArray(std::string(kServiceIdHash)));
    ASSERT_NE(socket_, nullptr);
  }

  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
  FakeBleL2capSocketImpl* fake_l2cap_socket_impl_;
  std::unique_ptr<BleSocket> socket_;
};

TEST_F(BleL2capSocketBleMediumTest, CloseSucceeds) {
  EXPECT_TRUE(socket_->Close().Ok());
}

TEST_F(BleL2capSocketBleMediumTest, CloseFails_PropagatesException) {
  fake_l2cap_socket_impl_->SetCloseException({Exception::kIo});

  Exception result = socket_->Close();

  EXPECT_EQ(result.value, Exception::kIo);
}

TEST_F(BleL2capSocketBleMediumTest, GetInputStreamReturnsSameStreamInstance) {
  InputStream& stream1 = socket_->GetInputStream();
  InputStream& stream2 = socket_->GetInputStream();

  EXPECT_EQ(&stream1, &stream2);
}

TEST_F(BleL2capSocketBleMediumTest, GetOutputStreamReturnsSameStreamInstance) {
  OutputStream& stream1 = socket_->GetOutputStream();
  OutputStream& stream2 = socket_->GetOutputStream();

  EXPECT_EQ(&stream1, &stream2);
}

TEST_F(BleL2capSocketBleMediumTest, GetRemotePeripheralReturnsSameInstance) {
  nearby::BlePeripheral& peripheral1 = socket_->GetRemotePeripheral();
  nearby::BlePeripheral& peripheral2 = socket_->GetRemotePeripheral();

  EXPECT_EQ(&peripheral1, &peripheral2);
}

TEST_F(BleL2capSocketBleMediumTest, DispatchPacketWithCorrectServiceIdHash) {
  fake_input_stream_.Append(ByteArray(std::string(kServiceIdHash)));
  auto result = socket_->DispatchPacket();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray(std::string(kServiceIdHash)));
}

TEST_F(BleL2capSocketBleMediumTest, DispatchPacketWithWrongServiceIdHash) {
  fake_input_stream_.Append(ByteArray("\x01\x02\x03"));
  auto result = socket_->DispatchPacket();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kFailed);
}

TEST_F(BleL2capSocketBleMediumTest, DispatchPacketWithControlPacket) {
  auto control_packet_status = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(control_packet_status.ok());
  BlePacket control_packet = control_packet_status.value();
  fake_input_stream_.Append(ByteArray(control_packet));
  fake_input_stream_.Append(ByteArray(std::string(kServiceIdHash)));

  auto result = socket_->DispatchPacket();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), ByteArray(std::string(kServiceIdHash)));
}

TEST_F(BleL2capSocketBleMediumTest, DispatchPacketWithReadError) {
  fake_input_stream_.SetExceptionOnRead(true);
  auto result = socket_->DispatchPacket();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kIo);
}

TEST_F(BleL2capSocketBleMediumTest, ReadPayloadLengthSuccess) {
  constexpr int kPayloadLength = 12345;
  fake_input_stream_.Append(ByteArray(byte_utils::IntToBytes(kPayloadLength)));
  auto result = socket_->ReadPayloadLength();
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.result(), kPayloadLength);
}

TEST_F(BleL2capSocketBleMediumTest, ReadPayloadLengthReadError) {
  fake_input_stream_.SetExceptionOnRead(true);
  auto result = socket_->ReadPayloadLength();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kIo);
}

TEST_F(BleL2capSocketBleMediumTest, ReadPayloadLengthAfterClose) {
  socket_->Close();
  auto result = socket_->ReadPayloadLength();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kIo);
}

TEST_F(BleL2capSocketBleMediumTest, WritePayloadLengthSuccess) {
  constexpr int kPayloadLength = 12345;
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleL2capSocketBleMediumTest, WritePayloadLengthAfterClose) {
  constexpr int kPayloadLength = 12345;
  socket_->Close();
  EXPECT_FALSE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleL2capSocketBleMediumTest, WritePayloadLengthFailsIfCalledTwice) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kRefactorBleL2cap,
      true);
  constexpr int kPayloadLength = 12345;
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
  EXPECT_FALSE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       WritePayloadLengthSucceedsIfCalledAfterWrite) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kRefactorBleL2cap,
      true);
  constexpr int kPayloadLength = 12345;
  ByteArray payload("payload");
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
  EXPECT_TRUE(socket_->GetOutputStream().Write(payload).Ok());
  EXPECT_TRUE(socket_->WritePayloadLength(kPayloadLength).Ok());
}

TEST_F(BleL2capSocketBleMediumTest, IsValid) {
  EXPECT_TRUE(socket_->IsValid());
  EXPECT_TRUE(socket_->Close().Ok());
  EXPECT_TRUE(socket_->IsValid());
}

TEST_F(BleL2capSocketBleMediumTest, SendIntroductionSuccess) {
  EXPECT_TRUE(socket_->SendIntroduction().Ok());
  auto intro_packet = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(intro_packet.ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(ByteArray(*intro_packet)));
}

TEST_F(BleL2capSocketBleMediumTest, SendIntroductionAfterClose) {
  socket_->Close();
  EXPECT_FALSE(socket_->SendIntroduction().Ok());
}

TEST_F(BleL2capSocketBleMediumTest, SendIntroductionWriteFails) {
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->SendIntroduction().Ok());
}

TEST_F(BleL2capSocketBleMediumTest, GetMedium) {
  EXPECT_EQ(socket_->GetMedium(), Medium::BLE_L2CAP);
}

TEST_F(BleL2capSocketBleMediumTest, SendDisconnectionSuccess) {
  EXPECT_TRUE(socket_->SendDisconnection().Ok());
  auto disconnection_packet = BlePacket::CreateControlDisconnectionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(disconnection_packet.ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(ByteArray(*disconnection_packet)));
}

TEST_F(BleL2capSocketBleMediumTest, SendDisconnectionAfterClose) {
  socket_->Close();
  EXPECT_FALSE(socket_->SendDisconnection().Ok());
}

TEST_F(BleL2capSocketBleMediumTest, SendDisconnectionWriteFails) {
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->SendDisconnection().Ok());
}

TEST_F(BleL2capSocketBleMediumTest, SendPacketAcknowledgementSuccess) {
  constexpr int kReceivedSize = 100;
  EXPECT_TRUE(socket_->SendPacketAcknowledgement(kReceivedSize).Ok());
  auto ack_packet = BlePacket::CreateControlPacketAcknowledgementPacket(
      ByteArray(std::string(kServiceIdHash)), kReceivedSize);
  ASSERT_TRUE(ack_packet.ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(ByteArray(*ack_packet)));
}

TEST_F(BleL2capSocketBleMediumTest, SendPacketAcknowledgementAfterClose) {
  constexpr int kReceivedSize = 100;
  socket_->Close();
  EXPECT_FALSE(socket_->SendPacketAcknowledgement(kReceivedSize).Ok());
}

TEST_F(BleL2capSocketBleMediumTest, SendPacketAcknowledgementWriteFails) {
  constexpr int kReceivedSize = 100;
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->SendPacketAcknowledgement(kReceivedSize).Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessIncomingL2capPacketValidationSuccess) {
  fake_input_stream_.Append(
      BleL2capPacket::ByteArrayForRequestDataConnection());
  EXPECT_TRUE(socket_->ProcessIncomingL2capPacketValidation().Ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(BleL2capPacket::ByteArrayForDataConnectionReady()));
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessIncomingL2capPacketValidationReadFails) {
  fake_input_stream_.SetExceptionOnRead(true);
  EXPECT_FALSE(socket_->ProcessIncomingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessIncomingL2capPacketValidationWrongPacket) {
  fake_input_stream_.Append(BleL2capPacket::ByteArrayForDataConnectionReady());
  EXPECT_FALSE(socket_->ProcessIncomingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessIncomingL2capPacketValidationWriteFails) {
  fake_input_stream_.Append(
      BleL2capPacket::ByteArrayForRequestDataConnection());
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->ProcessIncomingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessIncomingL2capPacketValidationAfterClose) {
  socket_->Close();
  EXPECT_FALSE(socket_->ProcessIncomingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessOutgoingL2capPacketValidationSuccess) {
  fake_input_stream_.Append(BleL2capPacket::ByteArrayForDataConnectionReady());
  EXPECT_TRUE(socket_->ProcessOutgoingL2capPacketValidation().Ok());
  EXPECT_EQ(fake_output_stream_.GetPayload(),
            std::string(BleL2capPacket::ByteArrayForRequestDataConnection()));
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessOutgoingL2capPacketValidationWriteFails) {
  fake_output_stream_.SetExceptionOnWrite(true);
  EXPECT_FALSE(socket_->ProcessOutgoingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessOutgoingL2capPacketValidationReadFails) {
  fake_input_stream_.SetExceptionOnRead(true);
  EXPECT_FALSE(socket_->ProcessOutgoingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessOutgoingL2capPacketValidationWrongPacket) {
  fake_input_stream_.Append(
      BleL2capPacket::ByteArrayForRequestDataConnection());
  EXPECT_FALSE(socket_->ProcessOutgoingL2capPacketValidation().Ok());
}

TEST_F(BleL2capSocketBleMediumTest,
       ProcessOutgoingL2capPacketValidationAfterClose) {
  socket_->Close();
  EXPECT_FALSE(socket_->ProcessOutgoingL2capPacketValidation().Ok());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
