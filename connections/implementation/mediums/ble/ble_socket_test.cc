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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

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

class BleL2capSocketBleMediumTest : public ::testing::Test {
 protected:
  void SetUp() override {
    nearby::BlePeripheral peripheral;
    auto fake_l2cap_socket_impl =
        std::make_unique<FakeBleL2capSocketImpl>(
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

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
