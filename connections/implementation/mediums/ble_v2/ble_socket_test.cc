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

#include "connections/implementation/mediums/ble_v2/ble_socket.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/mediums/ble_v2/ble_packet.h"
#include "connections/implementation/mediums/ble_v2/mock_ble_socket.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

struct BleSocketTestParams {
  bool is_l2cap_socket;
};

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

constexpr absl::string_view kServiceIdHash{"\x0a\x0b\x0c"};

class FakeInputStream : public InputStream {
 public:
  explicit FakeInputStream(const ByteArray& data) : data_(data) {}
  ~FakeInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    if (size < 0) return {Exception::kIo};
    if (read_from_ >= data_.size()) return {Exception::kIo};
    size_t available = static_cast<size_t>(data_.size() - read_from_);
    size_t size_to_read = std::min(static_cast<size_t>(size), available);
    ByteArray result(data_.data() + read_from_,
                     static_cast<size_t>(size_to_read));
    read_from_ += static_cast<size_t>(size_to_read);
    return ExceptionOr<ByteArray>(result);
  }

  Exception Close() override { return Exception(Exception::kSuccess); }

 private:
  ByteArray data_;
  size_t read_from_{0};
};

class FakeOutputStream : public OutputStream {
 public:
  Exception Write(const ByteArray& data) override {
    buffer_ = data;
    return {Exception::kSuccess};
  }
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override { return {Exception::kSuccess}; }

  // Test-only method:
  // Returns a copy of all data written to the stream.
  ByteArray GetWrittenData() const { return buffer_; };

 private:
  ByteArray buffer_{};
};

class BleSocketTest : public ::testing::TestWithParam<BleSocketTestParams> {
 protected:
  // Helper function to set up a socket with the specific inputs.
  std::unique_ptr<BleSocket> CreateTestSocket(bool is_l2cap,
                                              const ByteArray& request_packet) {
    // The FakeInputStream must exist for the duration of the test, so we
    // hold it in a member variable.
    fake_input_stream_ = std::make_unique<FakeInputStream>(request_packet);
    fake_output_stream_ = std::make_unique<FakeOutputStream>();

    if (is_l2cap) {
      auto mock_l2cap_socket_impl =
          std::make_unique<StrictMock<testing::MockBleL2capSocket>>();
      mock_platform_l2cap_socket_impl_ = mock_l2cap_socket_impl.get();

      // Set expectations on the mock socket.
      EXPECT_CALL(*mock_platform_l2cap_socket_impl_, GetInputStream())
          .WillRepeatedly(ReturnRef(*fake_input_stream_));
      EXPECT_CALL(*mock_platform_l2cap_socket_impl_, GetOutputStream())
          .WillRepeatedly(ReturnRef(*fake_output_stream_));
      EXPECT_CALL(*mock_platform_l2cap_socket_impl_, Close())
          .WillRepeatedly(Return(Exception{Exception::kSuccess}));

      BleL2capSocket platform_l2cap_socket(peripheral_,
                                           std::move(mock_l2cap_socket_impl));
      return BleSocket::CreateWithL2capSocket(
          std::move(platform_l2cap_socket),
          ByteArray(std::string(kServiceIdHash)));
    } else {
      auto mock_socket_impl =
          std::make_unique<StrictMock<testing::MockBleSocket>>();
      mock_platform_socket_impl_ = mock_socket_impl.get();

      // Set expectations on the mock socket.
      EXPECT_CALL(*mock_platform_socket_impl_, GetInputStream())
          .WillRepeatedly(ReturnRef(*fake_input_stream_));
      EXPECT_CALL(*mock_platform_socket_impl_, GetOutputStream())
          .WillRepeatedly(ReturnRef(*fake_output_stream_));
      EXPECT_CALL(*mock_platform_socket_impl_, Close())
          .WillRepeatedly(Return(Exception{Exception::kSuccess}));

      BleV2Socket platform_socket(peripheral_, std::move(mock_socket_impl));
      return BleSocket::CreateWithBleSocket(
          std::move(platform_socket), ByteArray(std::string(kServiceIdHash)));
    }
  }

  BleV2Peripheral peripheral_;
  StrictMock<testing::MockBleL2capSocket>* mock_platform_l2cap_socket_impl_;
  StrictMock<testing::MockBleSocket>* mock_platform_socket_impl_;
  std::unique_ptr<InputStream> fake_input_stream_;
  std::unique_ptr<FakeOutputStream> fake_output_stream_;
};

TEST_P(BleSocketTest, CloseSucceeds) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());

  EXPECT_TRUE(socket->Close().Ok());
}

TEST_P(BleSocketTest, CloseFails_PropagatesException) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());

  if (params.is_l2cap_socket) {
    EXPECT_CALL(*mock_platform_l2cap_socket_impl_, Close())
        .WillRepeatedly(Return(Exception{Exception::kIo}));
  } else {
    EXPECT_CALL(*mock_platform_socket_impl_, Close())
        .WillRepeatedly(Return(Exception{Exception::kIo}));
  }

  Exception result = socket->Close();

  EXPECT_EQ(result.value, Exception::kIo);
}

TEST_P(BleSocketTest, GetInputStreamReturnsSameStreamInstance) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());
  InputStream& stream1 = socket->GetInputStream();
  InputStream& stream2 = socket->GetInputStream();

  EXPECT_EQ(&stream1, &stream2);
}

TEST_P(BleSocketTest, GetOutputStreamReturnsSameStreamInstance) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());
  OutputStream& stream1 = socket->GetOutputStream();
  OutputStream& stream2 = socket->GetOutputStream();

  EXPECT_EQ(&stream1, &stream2);
}

TEST_P(BleSocketTest, GetRemotePeripheralReturnsSameInstance) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());
  BleV2Peripheral& peripheral1 = socket->GetRemotePeripheral();
  BleV2Peripheral& peripheral2 = socket->GetRemotePeripheral();

  EXPECT_EQ(&peripheral1, &peripheral2);
}

TEST_P(BleSocketTest, DispatchPacketSkipsControlPacketAndReturnsServiceIdHash) {
  BleSocketTestParams params = GetParam();

  const ByteArray service_id_hash{std::string(kServiceIdHash)};
  auto introduction_packet = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(introduction_packet.ok());

  const std::string data_section = "data_section";
  std::string data_packet = std::string(kServiceIdHash) + data_section;
  std::string combined_data =
      std::string(ByteArray(introduction_packet.value())) + data_packet;
  ByteArray combined_byte_array(combined_data);

  auto socket = CreateTestSocket(params.is_l2cap_socket, combined_byte_array);
  ASSERT_NE(socket, nullptr);

  auto result = socket->DispatchPacket();
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.result().AsStringView(), kServiceIdHash);
}

TEST_P(BleSocketTest, DispatchPacketFailsOnIncompletePacket) {
  BleSocketTestParams params = GetParam();

  auto introduction_packet = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(introduction_packet.ok());
  ByteArray complete_packet(introduction_packet.value());
  ByteArray incomplete_packet(complete_packet.data(), 4);

  auto socket = CreateTestSocket(params.is_l2cap_socket, incomplete_packet);
  ASSERT_NE(socket, nullptr);

  auto result = socket->DispatchPacket();
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.exception(), Exception::kFailed);
}

TEST_P(BleSocketTest, SendIntroductionSucceeds) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());
  ASSERT_NE(socket, nullptr);
  socket->SendIntroduction();

  // Verify that the correct disconnection control packet was written.
  auto introduction_packet = BlePacket::CreateControlIntroductionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(introduction_packet.ok());
  ByteArray expected_bytes = ByteArray(introduction_packet.value());
  EXPECT_EQ(fake_output_stream_->GetWrittenData(), expected_bytes);
}

TEST_P(BleSocketTest, SendDisconnectionSucceeds) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());
  ASSERT_NE(socket, nullptr);
  socket->SendDisconnection();

  // Verify that the correct disconnection control packet was written.
  auto disconnection_packet = BlePacket::CreateControlDisconnectionPacket(
      ByteArray(std::string(kServiceIdHash)));
  ASSERT_TRUE(disconnection_packet.ok());
  ByteArray expected_bytes = ByteArray(disconnection_packet.value());
  EXPECT_EQ(fake_output_stream_->GetWrittenData(), expected_bytes);
}

TEST_P(BleSocketTest, SendPacketAcknowledgementSucceeds) {
  BleSocketTestParams params = GetParam();

  auto socket = CreateTestSocket(params.is_l2cap_socket, ByteArray());
  ASSERT_NE(socket, nullptr);
  const int received_size = 10;
  socket->SendPacketAcknowledgement(received_size);

  // Verify that the correct acknowledgement packet was written.
  auto ack_packet_status = BlePacket::CreateControlPacketAcknowledgementPacket(
      ByteArray(std::string(kServiceIdHash)), received_size);
  ASSERT_TRUE(ack_packet_status.ok());
  ByteArray expected_bytes = ByteArray(ack_packet_status.value());
  EXPECT_EQ(fake_output_stream_->GetWrittenData(), expected_bytes);
}

INSTANTIATE_TEST_SUITE_P(ParametrisedBleSocketTest, BleSocketTest,
                         ::testing::ValuesIn<BleSocketTestParams>({
                             {.is_l2cap_socket = true},
                             {.is_l2cap_socket = false},
                         }));

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
