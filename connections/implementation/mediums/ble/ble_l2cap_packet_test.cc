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

#include "connections/implementation/mediums/ble/ble_l2cap_packet.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/pipe.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr absl::string_view kInCorrectData = {"\x01\x02\x03\x04\x05"};
constexpr absl::string_view kServiceID = "1test_service_id";
constexpr absl::string_view kCorrectData = {
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x00"};

class SingleByteInputStream : public InputStream {
 public:
  explicit SingleByteInputStream(absl::string_view data) : data_(data) {}
  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    if (pos_ >= data_.size() || size <= 0) {
      return ExceptionOr<ByteArray>{ByteArray{}};
    }
    std::size_t chunk = std::min<std::size_t>(1, data_.size() - pos_);
    ByteArray out(data_.data() + pos_, chunk);
    pos_ += chunk;
    return ExceptionOr<ByteArray>{out};
  }
  Exception Close() override { return {Exception::kSuccess}; }

 private:
  std::string data_;
  std::size_t pos_ = 0;
};

TEST(BleL2capPacketTest, ByteArrayForRequestAdvertisementWithCorrectServiceID) {
  auto byte_array =
      BleL2capPacket::ByteArrayForRequestAdvertisement(std::string(kServiceID));

  ASSERT_TRUE(byte_array.ok());

  auto result = BleL2capPacket::CreateFromBytes(*byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsFetchAdvertisementRequest());
  ASSERT_EQ(result.value().GetServiceIdHash(),
            BleL2capPacket::GenerateServiceIdHash(std::string(kServiceID)));
}

TEST(BleL2capPacketTest, ByteArrayForRequestAdvertisementWithEmptyServiceID) {
  auto byte_array = BleL2capPacket::ByteArrayForRequestAdvertisement("");

  ASSERT_FALSE(byte_array.ok());
}

TEST(BleL2capPacketTest,
     ByteArrayForResponseAdvertisementWithCorrectAdvertisement) {
  ByteArray advertisement(kCorrectData.data(), kCorrectData.size());
  auto byte_array =
      BleL2capPacket::ByteArrayForResponseAdvertisement(advertisement);

  ASSERT_TRUE(byte_array.ok());

  auto result = BleL2capPacket::CreateFromBytes(*byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsAdvertisementResponse());
  ASSERT_EQ(result.value().GetAdvertisement(), advertisement);
}

TEST(BleL2capPacketTest,
     ByteArrayForResponseAdvertisementWithInCorrectAdvertisement) {
  ByteArray advertisement(kInCorrectData.data(), kInCorrectData.size());
  auto byte_array =
      BleL2capPacket::ByteArrayForResponseAdvertisement(advertisement);

  ASSERT_FALSE(byte_array.ok());
}

TEST(BleL2capPacketTest,
     ByteArrayForResponseAdvertisementWithLargeAdvertisement) {
  auto byte_array =
      BleL2capPacket::ByteArrayForResponseAdvertisement(ByteArray(300));

  ASSERT_TRUE(byte_array.ok());

  auto result = BleL2capPacket::CreateFromBytes(byte_array.value());
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsAdvertisementResponse());
  ASSERT_EQ(result.value().GetAdvertisement().size(), 300);
}

TEST(BleL2capPacketTest, ByteArrayForRequestAdvertisementFinish) {
  ByteArray byte_array =
      BleL2capPacket::ByteArrayForRequestAdvertisementFinish();

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsFetchAdvertisementFinished());
}

TEST(BleL2capPacketTest, ByteArrayForServiceIdNotFound) {
  ByteArray byte_array = BleL2capPacket::ByteArrayForServiceIdNotFound();

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsErrorServiceIdNotFound());
}

TEST(BleL2capPacketTest, ByteArrayForRequestDataConnection) {
  ByteArray byte_array = BleL2capPacket::ByteArrayForRequestDataConnection();

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsDataConnectionRequest());
}

TEST(BleL2capPacketTest, ByteArrayForDataConnectionReady) {
  ByteArray byte_array = BleL2capPacket::ByteArrayForDataConnectionReady();

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsDataConnectionReadyResponse());
}

TEST(BleL2capPacketTest, ByteArrayForDataConnectionFailure) {
  ByteArray byte_array = BleL2capPacket::ByteArrayForDataConnectionFailure();

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_TRUE(result.ok());
  ASSERT_TRUE(result.value().IsDataConnectionFailureResponse());
}

TEST(BleL2capPacketTest, CreateFromBytesWithEmptyData) {
  auto byte_array = BleL2capPacket::CreateFromBytes(ByteArray());

  ASSERT_FALSE(byte_array.ok());
}

TEST(BleL2capPacketTest, CreateFromBytesWithInvalidCommand) {
  ByteArray byte_array(nullptr, 1);

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_FALSE(result.ok());
}

TEST(BleL2capPacketTest, CreateFromBytesWithoutDataLength) {
  ByteArray byte_array{1};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement);

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_FALSE(result.ok());
}

TEST(BleL2capPacketTest, CreateFromBytesWithEmptyDataLength) {
  ByteArray byte_array{3};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement);
  byte_array_data[1] = 0x00;
  byte_array_data[2] = 0x00;

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_FALSE(result.ok());
}

TEST(BleL2capPacketTest, CreateFromBytesWithInvalidServiceIdHashLength) {
  ByteArray byte_array{3};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement);
  byte_array_data[1] = 0x00;
  byte_array_data[2] = 0x01;

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_FALSE(result.ok());
}

TEST(BleL2capPacketTest, CreateFromBytesWithTooLongServiceIdHashLength) {
  ByteArray byte_array{7};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement);
  byte_array_data[1] = 0x00;
  byte_array_data[2] = 0x04;

  EXPECT_THAT(BleL2capPacket::CreateFromBytes(byte_array),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("service id hash length incorrect")));
}

TEST(BleL2capPacketTest, CreateFromBytesWithTooLongAdvertisementLength) {
  ByteArray byte_array{3};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kResponseAdvertisement);
  byte_array_data[1] = 0xFF;
  byte_array_data[2] = 0xFF;

  absl::StatusOr<BleL2capPacket> result =
      BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_FALSE(result.ok());
}

TEST(BleL2capPacketTest, CreateFromBytesWithInvalidLengthAdvertisement) {
  ByteArray byte_array{4};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kResponseAdvertisement);
  byte_array_data[1] = 0x00;
  byte_array_data[2] = 0x03;
  byte_array_data[3] = 0x01;

  EXPECT_THAT(BleL2capPacket::CreateFromBytes(byte_array),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("advertisement length incorrect")));
}

TEST(BleL2capPacketTest, CreateFromBytesWithTruncatedAdvertisementPayload) {
  ByteArray byte_array{4};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kResponseAdvertisement);
  byte_array_data[1] = 0x00;
  byte_array_data[2] = 0x0a;
  byte_array_data[3] = 0x01;

  EXPECT_THAT(BleL2capPacket::CreateFromBytes(byte_array),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("advertisement not available")));
}

TEST(BleL2capPacketTest, CreateFromStreamFailsReadCommand) {
  auto [input, output] = nearby::CreatePipe();

  output->Write("");
  output->Close();

  EXPECT_THAT(BleL2capPacket::CreateFromStream(*input),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("command byte not available")));
}

TEST(BleL2capPacketTest,
     CreateFromStreamFailsEofLengthForRequestAdvertisement) {
  auto [input, output] = nearby::CreatePipe();
  std::string out(
      1, static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement));

  output->Write(out);
  output->Close();

  EXPECT_THAT(BleL2capPacket::CreateFromStream(*input),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("length byte not available")));
}

TEST(BleL2capPacketTest, CreateFromStreamFailsTruncatedOneByteLength) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(
          1, static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement)),
      "\x03");

  output->Write(out);
  output->Close();

  EXPECT_THAT(BleL2capPacket::CreateFromStream(*input),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("length byte not available")));
}

TEST(BleL2capPacketTest,
     CreateFromStreamFailsEofLengthForResponseAdvertisement) {
  auto [input, output] = nearby::CreatePipe();
  std::string out(
      1, static_cast<char>(BleL2capPacket::Command::kResponseAdvertisement));

  output->Write(out);
  output->Close();

  EXPECT_THAT(BleL2capPacket::CreateFromStream(*input),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("length byte not available")));
}

TEST(BleL2capPacketTest, CreateFromStreamSucceedsAcrossSingleByteChunkedReads) {
  ByteArray advertisement(300);
  absl::StatusOr<ByteArray> serialized =
      BleL2capPacket::ByteArrayForResponseAdvertisement(advertisement);
  ASSERT_TRUE(serialized.ok());

  SingleByteInputStream chunked_stream(serialized->AsStringView());
  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(chunked_stream);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsAdvertisementResponse());
  EXPECT_EQ(ble_l2cap_packet->GetAdvertisement(), advertisement);
}

TEST(BleL2capPacketTest, CreateFromStreamRequestAdvertisement) {
  auto [input, output] = nearby::CreatePipe();
  absl::StatusOr<ByteArray> byte_array =
      BleL2capPacket::ByteArrayForRequestAdvertisement(std::string(kServiceID));
  ASSERT_TRUE(byte_array.ok());

  output->Write(byte_array->AsStringView());
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsFetchAdvertisementRequest());
  EXPECT_EQ(ble_l2cap_packet->GetServiceIdHash(),
            BleL2capPacket::GenerateServiceIdHash(std::string(kServiceID)));
}

TEST(BleL2capPacketTest, CreateFromStreamZeroDataLength) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(
          1, static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement)),
      absl::string_view("\x00\x00", 2));

  output->Write(out);
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_FALSE(ble_l2cap_packet.ok());
}

TEST(BleL2capPacketTest,
     CreateFromStreamRequestAdvertisementBadServiceIdHashLength) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(
          1, static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement)),
      absl::string_view("\x00\x01", 2));

  output->Write(out);
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_FALSE(ble_l2cap_packet.ok());
}

TEST(BleL2capPacketTest,
     CreateFromStreamRequestAdvertisementTooLongServiceIdHashLength) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(
          1, static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement)),
      "\xFF\xFF");

  output->Write(out);

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  output->Close();
  EXPECT_THAT(ble_l2cap_packet,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("service id hash length incorrect")));
}

TEST(BleL2capPacketTest, CreateFromStreamFailsReadServiceIdHash) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(
          1, static_cast<char>(BleL2capPacket::Command::kRequestAdvertisement)),
      absl::string_view("\x00\x03", 2));

  output->Write(out);
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_FALSE(ble_l2cap_packet.ok());
}

TEST(BleL2capPacketTest, CreateFromStreamRequestAdvertisementFinish) {
  auto [input, output] = nearby::CreatePipe();
  ByteArray byte_array =
      BleL2capPacket::ByteArrayForRequestAdvertisementFinish();

  output->Write(byte_array.AsStringView());
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsFetchAdvertisementFinished());
}

TEST(BleL2capPacketTest, CreateFromStreamResponseAdvertisement) {
  auto [input, output] = nearby::CreatePipe();
  ByteArray advertisement(kCorrectData.data(), kCorrectData.size());
  absl::StatusOr<ByteArray> byte_array =
      BleL2capPacket::ByteArrayForResponseAdvertisement(advertisement);
  ASSERT_TRUE(byte_array.ok());

  output->Write(byte_array->AsStringView());
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsAdvertisementResponse());
  EXPECT_EQ(ble_l2cap_packet->GetAdvertisement(), advertisement);
}

TEST(BleL2capPacketTest,
     CreateFromStreamResponseAdvertisementBadAdvertisementLength) {
  auto [input, output] = nearby::CreatePipe();
  // 0xFFFF is too large for advertisement length.
  std::string out = absl::StrCat(
      std::string(1, static_cast<char>(
                         BleL2capPacket::Command::kResponseAdvertisement)),
      "\xFF\xFF");

  output->Write(out);
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_FALSE(ble_l2cap_packet.ok());
}

TEST(BleL2capPacketTest,
     CreateFromStreamResponseAdvertisementTooShortAdvertisementLength) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(1, static_cast<char>(
                         BleL2capPacket::Command::kResponseAdvertisement)),
      absl::string_view("\x00\x01", 2));

  output->Write(out);

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  output->Close();
  EXPECT_THAT(ble_l2cap_packet,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("advertisement length incorrect")));
}

TEST(BleL2capPacketTest, CreateFromStreamFailsReadAdvertisement) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(
      std::string(1, static_cast<char>(
                         BleL2capPacket::Command::kResponseAdvertisement)),
      absl::string_view("\x00\x0a", 2));

  output->Write(out);
  output->Close();

  absl::StatusOr<BleL2capPacket> ble_l2cap_packet =
      BleL2capPacket::CreateFromStream(*input);
  ASSERT_FALSE(ble_l2cap_packet.ok());
}

TEST(BleL2capPacketTest, CreateFromStreamResponseLargeAdvertisement) {
  auto [input, output] = nearby::CreatePipe();
  ByteArray advertisement(300);
  auto byte_array =
      BleL2capPacket::ByteArrayForResponseAdvertisement(advertisement);
  ASSERT_TRUE(byte_array.ok());

  output->Write(byte_array->AsStringView());
  output->Close();

  auto ble_l2cap_packet = BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsAdvertisementResponse());
  EXPECT_EQ(ble_l2cap_packet->GetAdvertisement(), advertisement);
  EXPECT_EQ(ble_l2cap_packet->GetAdvertisement().size(), 300);
}

TEST(BleL2capPacketTest, CreateFromStreamServiceIdNotFound) {
  auto [input, output] = nearby::CreatePipe();
  auto byte_array = BleL2capPacket::ByteArrayForServiceIdNotFound();

  output->Write(byte_array.AsStringView());
  output->Close();

  auto ble_l2cap_packet = BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsErrorServiceIdNotFound());
}

TEST(BleL2capPacketTest, CreateFromStreamRequestDataConnection) {
  auto [input, output] = nearby::CreatePipe();
  auto byte_array = BleL2capPacket::ByteArrayForRequestDataConnection();

  output->Write(byte_array.AsStringView());
  output->Close();

  auto ble_l2cap_packet = BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsDataConnectionRequest());
}

TEST(BleL2capPacketTest, CreateFromStreamDataConnectionReady) {
  auto [input, output] = nearby::CreatePipe();
  auto byte_array = BleL2capPacket::ByteArrayForDataConnectionReady();

  output->Write(byte_array.AsStringView());
  output->Close();

  auto ble_l2cap_packet = BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsDataConnectionReadyResponse());
}

TEST(BleL2capPacketTest, CreateFromStreamDataConnectionFailure) {
  auto [input, output] = nearby::CreatePipe();
  auto byte_array = BleL2capPacket::ByteArrayForDataConnectionFailure();

  output->Write(byte_array.AsStringView());
  output->Close();

  auto ble_l2cap_packet = BleL2capPacket::CreateFromStream(*input);
  ASSERT_TRUE(ble_l2cap_packet.ok());
  EXPECT_TRUE(ble_l2cap_packet->IsDataConnectionFailureResponse());
}

TEST(BleL2capPacketTest, CreateFromStreamUnsupportedCommand) {
  auto [input, output] = nearby::CreatePipe();
  std::string out = absl::StrCat(std::string(1, static_cast<char>(0XFF)));

  output->Write(out);
  output->Close();

  auto ble_l2cap_packet = BleL2capPacket::CreateFromStream(*input);
  ASSERT_FALSE(ble_l2cap_packet.ok());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
