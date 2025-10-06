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

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr absl::string_view kInCorrectData = {"\x01\x02\x03\x04\x05"};
constexpr absl::string_view kServiceID = "1test_service_id";
constexpr absl::string_view kCorrectData = {
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x00"};

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

TEST(BleL2capPacketTest, CreateFromBytesWithTooLongAdvertisementLength) {
  ByteArray byte_array{3};
  char* byte_array_data = byte_array.data();
  byte_array_data[0] =
      static_cast<char>(BleL2capPacket::Command::kResponseAdvertisement);
  byte_array_data[1] = 0xFF;
  byte_array_data[2] = 0xFF;

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
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

  auto result = BleL2capPacket::CreateFromBytes(byte_array);
  ASSERT_FALSE(result.ok());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
