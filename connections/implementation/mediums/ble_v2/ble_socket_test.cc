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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
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

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

constexpr absl::string_view kServiceIdHash{"\x0a\x0b\x0c"};

class FakeInputStream : public InputStream {
 public:
  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    return ExceptionOr<ByteArray>(Exception::kIo);
  }
  Exception Close() override { return {Exception::kSuccess}; }
};

class FakeOutputStream : public OutputStream {
 public:
  Exception Write(const ByteArray& data) override {
    return {Exception::kSuccess};
  }
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override { return {Exception::kSuccess}; }
};

class BleSocketBleMediumTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BleV2Peripheral peripheral;

    auto mock_socket_impl =
        std::make_unique<StrictMock<testing::MockBleSocket>>();
    mock_platform_socket_impl_ = mock_socket_impl.get();

    EXPECT_CALL(*mock_platform_socket_impl_, GetInputStream())
        .WillRepeatedly(ReturnRef(fake_input_stream_));
    EXPECT_CALL(*mock_platform_socket_impl_, GetOutputStream())
        .WillRepeatedly(ReturnRef(fake_output_stream_));

    EXPECT_CALL(*mock_platform_socket_impl_, Close())
        .WillRepeatedly(Return(Exception{Exception::kSuccess}));

    BleV2Socket platform_socket(peripheral, std::move(mock_socket_impl));

    socket_ = BleSocket::CreateWithBleSocket(
        std::move(platform_socket), ByteArray(std::string(kServiceIdHash)));
    ASSERT_NE(socket_, nullptr);
  }

  StrictMock<testing::MockBleSocket>* mock_platform_socket_impl_;
  std::unique_ptr<BleSocket> socket_;
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
};

TEST_F(BleSocketBleMediumTest, CloseSucceeds) {
  EXPECT_TRUE(socket_->Close().Ok());
}

TEST_F(BleSocketBleMediumTest, CloseFails_PropagatesException) {
  EXPECT_CALL(*mock_platform_socket_impl_, Close())
      .WillRepeatedly(Return(Exception{Exception::kIo}));

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
  BleV2Peripheral& peripheral1 = socket_->GetRemotePeripheral();
  BleV2Peripheral& peripheral2 = socket_->GetRemotePeripheral();

  EXPECT_EQ(&peripheral1, &peripheral2);
}

class BleL2capSocketBleMediumTest : public ::testing::Test {
 protected:
  void SetUp() override {
    BleV2Peripheral peripheral;
    auto mock_l2cap_socket_impl =
        std::make_unique<StrictMock<testing::MockBleL2capSocket>>();

    mock_platform_l2cap_socket_impl_ = mock_l2cap_socket_impl.get();

    EXPECT_CALL(*mock_platform_l2cap_socket_impl_, GetInputStream())
        .WillRepeatedly(ReturnRef(fake_input_stream_));
    EXPECT_CALL(*mock_platform_l2cap_socket_impl_, GetOutputStream())
        .WillRepeatedly(ReturnRef(fake_output_stream_));
    EXPECT_CALL(*mock_platform_l2cap_socket_impl_, Close())
        .WillRepeatedly(Return(Exception{Exception::kSuccess}));

    BleL2capSocket platform_l2cap_socket(peripheral,
                                         std::move(mock_l2cap_socket_impl));

    socket_ = BleSocket::CreateWithL2capSocket(
        std::move(platform_l2cap_socket),
        ByteArray(std::string(kServiceIdHash)));
    ASSERT_NE(socket_, nullptr);
  }

  StrictMock<testing::MockBleL2capSocket>* mock_platform_l2cap_socket_impl_;
  std::unique_ptr<BleSocket> socket_;
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
};

TEST_F(BleL2capSocketBleMediumTest, CloseSucceeds) {
  EXPECT_TRUE(socket_->Close().Ok());
}

TEST_F(BleL2capSocketBleMediumTest, CloseFails_PropagatesException) {
  EXPECT_CALL(*mock_platform_l2cap_socket_impl_, Close())
      .WillRepeatedly(Return(Exception{Exception::kIo}));

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
  BleV2Peripheral& peripheral1 = socket_->GetRemotePeripheral();
  BleV2Peripheral& peripheral2 = socket_->GetRemotePeripheral();

  EXPECT_EQ(&peripheral1, &peripheral2);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
