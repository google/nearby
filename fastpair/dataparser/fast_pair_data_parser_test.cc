// Copyright 2022 Google LLC
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

#include "fastpair/dataparser/fast_pair_data_parser.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/non_discoverable_advertisement.h"
#include "fastpair/crypto/fast_pair_encryption.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
namespace {
// Test data comes from:
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases

constexpr absl::string_view kModelId("aabbcc");
constexpr absl::string_view kInvalidModelId("aabb");
constexpr absl::string_view kDeviceAddress("11:12:13:14:15:16");
constexpr absl::string_view kAccountKeyFilter("112233445566");
constexpr absl::string_view kSalt("01");
constexpr absl::string_view kBattery("01048F");
constexpr int kBatteryHeader = 0b00110011;
constexpr std::array<uint8_t, kAesBlockByteSize> kAeskeyarray = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};
constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kSaltHeader = 0b00010001;

TEST(FastPairDataParserTest, GetHexModelIdFromServiceDataUnsucessfully) {
  const std::vector<uint8_t> service_data =
      FastPairServiceDataCreator::Builder()
          .SetModelId(kInvalidModelId)
          .Build()
          ->CreateServiceData();
  CountDownLatch latch(1);
  FastPairDataParser::GetHexModelIdFromServiceData(
      service_data, [&](std::optional<absl::string_view> model_id) {
        EXPECT_EQ(model_id, std::nullopt);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, GetHexModelIdFromServiceDataSuccessfully) {
  const std::vector<uint8_t> service_data =
      FastPairServiceDataCreator::Builder()
          .SetModelId(kModelId)
          .Build()
          ->CreateServiceData();
  CountDownLatch latch(1);
  FastPairDataParser::GetHexModelIdFromServiceData(
      service_data, [&](std::optional<absl::string_view> model_id) {
        EXPECT_EQ(model_id, kModelId);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptResponseUnsuccessfullyWithInvalidAesKey) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95,
                                             0x1F, 0xF7, 0xB6, 0xCF, 0x5E,
                                             0x3F, 0x45, 0x61, 0xC3, 0x32};

  const std::vector<uint8_t> kResponseBytes = {/*message_type=*/0x02,
                                               /*address_bytes=*/0x02,
                                               0x03,
                                               0x04,
                                               0x05,
                                               0x06,
                                               0x07,
                                               /*salt=*/0x08,
                                               0x09,
                                               0x0A,
                                               0x0B,
                                               0x0C,
                                               0x0D,
                                               0x0E,
                                               0x0F,
                                               0x00};

  std::array<uint8_t, kAesBlockByteSize> response_bytes_array;
  std::copy(kResponseBytes.begin(), kResponseBytes.end(),
            response_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, response_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end());

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedResponse(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedResponse> response) {
        EXPECT_FALSE(response.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptResponseUnsuccessfullyWithInvalidResponse) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                             0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                             0x61, 0xC3, 0x32, 0x1D};

  const std::vector<uint8_t> kResponseBytes = {/*message_type=*/0x02,
                                               /*address_bytes=*/0x02,
                                               0x03,
                                               0x04,
                                               0x05,
                                               0x06,
                                               0x07,
                                               /*salt=*/0x08,
                                               0x09,
                                               0x0A,
                                               0x0B,
                                               0x0C,
                                               0x0D,
                                               0x0E,
                                               0x0F,
                                               0x00};

  std::array<uint8_t, kAesBlockByteSize> response_bytes_array;
  std::copy(kResponseBytes.begin(), kResponseBytes.end(),
            response_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, response_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end() - 1);

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedResponse(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedResponse> response) {
        EXPECT_FALSE(response.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptResponseSuccessfully) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                             0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                             0x61, 0xC3, 0x32, 0x1D};
  std::vector<uint8_t> response_bytes;

  // Message type.
  constexpr uint8_t kMessageType = 0x01;
  response_bytes.push_back(kMessageType);

  // Address bytes.
  constexpr std::array<uint8_t, 6> kAddressBytes = {0x02, 0x03, 0x04,
                                                    0x05, 0x06, 0x07};
  std::copy(kAddressBytes.begin(), kAddressBytes.end(),
            std::back_inserter(response_bytes));

  // Random salt
  constexpr std::array<uint8_t, 9> kSalt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                            0x0D, 0x0E, 0x0F, 0x00};
  std::copy(kSalt.begin(), kSalt.end(), std::back_inserter(response_bytes));

  std::array<uint8_t, kAesBlockByteSize> response_bytes_array;
  std::copy(response_bytes.begin(), response_bytes.end(),
            response_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, response_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end());

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedResponse(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedResponse> response) {
        ASSERT_TRUE(response.has_value());
        EXPECT_EQ(response.value().message_type,
                  FastPairMessageType::kKeyBasedPairingResponse);
        EXPECT_EQ(response.value().address_bytes, kAddressBytes);
        EXPECT_EQ(response.value().salt, kSalt);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptPasskeyUnsuccessfullyWithInvalidAesKey) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95,
                                             0x1F, 0xF7, 0xB6, 0xCF, 0x5E,
                                             0x3F, 0x45, 0x61, 0xC3, 0x32};

  const std::vector<uint8_t> kPasskeyBytes = {/*message_type=*/0x02,
                                              /*address_bytes=*/0x02,
                                              0x03,
                                              0x04,
                                              0x05,
                                              0x06,
                                              0x07,
                                              /*salt=*/0x08,
                                              0x09,
                                              0x0A,
                                              0x0B,
                                              0x0C,
                                              0x0D,
                                              0x0E,
                                              0x0F,
                                              0x00};

  std::array<uint8_t, kAesBlockByteSize> passkey_bytes_array;
  std::copy(kPasskeyBytes.begin(), kPasskeyBytes.end(),
            passkey_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, passkey_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end());

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedPasskey(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedPasskey> decrypted_passkey) {
        EXPECT_FALSE(decrypted_passkey.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptPasskeyUnsuccessfullyWithInvalidPasskey) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                             0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                             0x61, 0xC3, 0x32, 0x1D};

  const std::vector<uint8_t> kPasskeyBytes = {/*message_type=*/0x04,
                                              /*passkey=*/0x02,
                                              0x03,
                                              0x04,
                                              /*salt=*/0x05,
                                              0x06,
                                              0x07,
                                              0x08,
                                              0x09,
                                              0x0A,
                                              0x0B,
                                              0x0C,
                                              0x0D,
                                              0x0E,
                                              0x0F};

  std::array<uint8_t, kAesBlockByteSize> passkey_bytes_array;
  std::copy(kPasskeyBytes.begin(), kPasskeyBytes.end(),
            passkey_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, passkey_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end() - 1);

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedPasskey(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedPasskey> decrypted_passkey) {
        EXPECT_FALSE(decrypted_passkey.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptSeekerPasskeySuccessfully) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                             0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                             0x61, 0xC3, 0x32, 0x1D};

  std::vector<uint8_t> passkey_bytes;
  // Message type.
  constexpr uint8_t kMessageType = 0x02;
  passkey_bytes.push_back(kMessageType);

  // Passkey bytes.
  constexpr uint32_t kPasskey = 5;
  passkey_bytes.push_back(kPasskey >> 16);
  passkey_bytes.push_back(kPasskey >> 8);
  passkey_bytes.push_back(kPasskey);

  // Random salt
  constexpr std::array<uint8_t, 12> kSalt = {
      0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E, 0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(kSalt.begin(), kSalt.end(), std::back_inserter(passkey_bytes));
  std::array<uint8_t, kAesBlockByteSize> passkey_bytes_array;
  std::copy(passkey_bytes.begin(), passkey_bytes.end(),
            passkey_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, passkey_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end());

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedPasskey(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedPasskey> decrypted_passkey) {
        ASSERT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey.value().message_type,
                  FastPairMessageType::kSeekersPasskey);
        EXPECT_EQ(decrypted_passkey.value().passkey, kPasskey);
        EXPECT_EQ(decrypted_passkey.value().salt, kSalt);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, DecryptProviderPasskeySuccessfully) {
  const std::vector<uint8_t> kAesKeyBytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                             0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                             0x61, 0xC3, 0x32, 0x1D};
  std::vector<uint8_t> passkey_bytes;
  // Message type.
  constexpr uint8_t kMessageType = 0x03;
  passkey_bytes.push_back(kMessageType);

  // Passkey bytes.
  constexpr uint32_t kPasskey = 5;
  passkey_bytes.push_back(kPasskey >> 16);
  passkey_bytes.push_back(kPasskey >> 8);
  passkey_bytes.push_back(kPasskey);

  // Random salt
  constexpr std::array<uint8_t, 12> kSalt = {
      0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E, 0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(kSalt.begin(), kSalt.end(), std::back_inserter(passkey_bytes));

  std::array<uint8_t, kAesBlockByteSize> passkey_bytes_array;
  std::copy(passkey_bytes.begin(), passkey_bytes.end(),
            passkey_bytes_array.begin());
  auto encrypted_bytes_array =
      FastPairEncryption::EncryptBytes(kAeskeyarray, passkey_bytes_array);
  std::vector<uint8_t> encrypted_bytes(encrypted_bytes_array.begin(),
                                       encrypted_bytes_array.end());

  CountDownLatch latch(1);
  FastPairDataParser::ParseDecryptedPasskey(
      kAesKeyBytes, encrypted_bytes,
      [&](std::optional<DecryptedPasskey> decrypted_passkey) {
        ASSERT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey.value().message_type,
                  FastPairMessageType::kProvidersPasskey);
        EXPECT_EQ(decrypted_passkey.value().passkey, kPasskey);
        EXPECT_EQ(decrypted_passkey.value().salt, kSalt);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementEmpty) {
  CountDownLatch latch(1);
  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      "", kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest,
     ParseNotDiscoverableAdvertisementNoApplicibleData) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);
  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest,
     ParseNotDiscoverableAdvertisementAccountKeyFilter) {
  const std::vector<uint8_t> kSaltBytes = {0x01};
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        ASSERT_TRUE(advertisement.has_value());
        EXPECT_EQ(absl::BytesToHexString(
                      std::string(advertisement->account_key_filter.begin(),
                                  advertisement->account_key_filter.end())),
                  kAccountKeyFilter);
        EXPECT_EQ(advertisement->salt, kSaltBytes);
        EXPECT_EQ(advertisement->type,
                  NonDiscoverableAdvertisement::Type::kShowUi);
        EXPECT_FALSE(advertisement->battery_notification.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest,
     ParseNotDiscoverableAdvertisementAccountKeyFilterNoNotification) {
  const std::vector<uint8_t> kSaltBytes = {0x01};
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(
              /*accountKeyFilterNoNotificationHeader*/ 0b01100010)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .Build()
          ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        ASSERT_TRUE(advertisement.has_value());
        EXPECT_EQ(absl::BytesToHexString(
                      std::string(advertisement->account_key_filter.begin(),
                                  advertisement->account_key_filter.end())),
                  kAccountKeyFilter);
        EXPECT_EQ(advertisement->salt, kSaltBytes);
        EXPECT_EQ(advertisement->type,
                  NonDiscoverableAdvertisement::Type::kHideUi);
        EXPECT_FALSE(advertisement->battery_notification.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementWrongVersion) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(/*InvalidHeader*/ 0b00100000)
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest,
     ParseNotDiscoverableAdvertisementZeroLengthExtraField) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField("")
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementWrongType) {
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader /*InvalidType*/ (0b01100001)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .Build()
          ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementSaltTwoBytes) {
  const std::vector<uint8_t> kLargeSaltBytes = {0xC7, 0xC8};
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(/*SaltTwoBytes*/ 0b00100001)
          .AddExtraField(/*LargeSalt*/ "C7C8")
          .Build()
          ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        ASSERT_TRUE(advertisement.has_value());
        EXPECT_EQ(absl::BytesToHexString(
                      std::string(advertisement->account_key_filter.begin(),
                                  advertisement->account_key_filter.end())),
                  kAccountKeyFilter);
        EXPECT_EQ(advertisement->salt, kLargeSaltBytes);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementSaltTooLarge) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(0b00110001)
                                   .AddExtraField(/*invalidSalt*/ "C7C8C9")
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementWithBattery) {
  const std::vector<uint8_t> kSaltBytes = {0x01};
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .AddExtraFieldHeader(kBatteryHeader)
                                   .AddExtraField(kBattery)
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        ASSERT_TRUE(advertisement.has_value());
        EXPECT_EQ(absl::BytesToHexString(
                      std::string(advertisement->account_key_filter.begin(),
                                  advertisement->account_key_filter.end())),
                  kAccountKeyFilter);
        EXPECT_EQ(advertisement->salt, kSaltBytes);
        EXPECT_EQ(advertisement->type,
                  NonDiscoverableAdvertisement::Type::kShowUi);
        ASSERT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_EQ(advertisement->battery_notification->type,
                  BatteryNotification::Type::kShowUi);
        EXPECT_FALSE(advertisement->battery_notification->battery_infos.at(0)
                         .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(0).percentage,
            1);
        EXPECT_FALSE(advertisement->battery_notification->battery_infos.at(1)
                         .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(1).percentage,
            4);
        ASSERT_TRUE(advertisement->battery_notification->battery_infos.at(2)
                        .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(2).percentage,
            15);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementMissingSalt) {
  const std::vector<uint8_t> kDeviceAddressBytes = {17, 18, 19, 20, 21, 22};
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kBatteryHeader)
                                   .AddExtraField(kBattery)
                                   .Build()
                                   ->CreateServiceData();
  CountDownLatch latch(1);
  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        ASSERT_TRUE(advertisement.has_value());
        EXPECT_EQ(absl::BytesToHexString(
                      std::string(advertisement->account_key_filter.begin(),
                                  advertisement->account_key_filter.end())),
                  kAccountKeyFilter);
        EXPECT_EQ(advertisement->salt, kDeviceAddressBytes);
        EXPECT_EQ(advertisement->type,
                  NonDiscoverableAdvertisement::Type::kShowUi);
        ASSERT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_EQ(advertisement->battery_notification->type,
                  BatteryNotification::Type::kShowUi);
        EXPECT_FALSE(advertisement->battery_notification->battery_infos.at(0)
                         .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(0).percentage,
            1);
        EXPECT_FALSE(advertisement->battery_notification->battery_infos.at(1)
                         .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(1).percentage,
            4);
        ASSERT_TRUE(advertisement->battery_notification->battery_infos.at(2)
                        .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(2).percentage,
            15);
        latch.CountDown();
      });
  latch.Await();
}

TEST(FastPairDataParserTest, ParseNotDiscoverableAdvertisementWithBatteryNoUi) {
  const std::vector<uint8_t> kSaltBytes = {0x01};
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .AddExtraFieldHeader(/*batterHeaderNoNotification*/ 0b00110100)
          .AddExtraField(kBattery)
          .Build()
          ->CreateServiceData();
  CountDownLatch latch(1);

  FastPairDataParser::ParseNotDiscoverableAdvertisement(
      std::string(bytes.begin(), bytes.end()), kDeviceAddress,
      [&](const std::optional<NonDiscoverableAdvertisement> advertisement) {
        ASSERT_TRUE(advertisement.has_value());
        EXPECT_EQ(absl::BytesToHexString(
                      std::string(advertisement->account_key_filter.begin(),
                                  advertisement->account_key_filter.end())),
                  kAccountKeyFilter);
        EXPECT_EQ(advertisement->salt, kSaltBytes);
        EXPECT_EQ(advertisement->type,
                  NonDiscoverableAdvertisement::Type::kShowUi);
        ASSERT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_EQ(advertisement->battery_notification->type,
                  BatteryNotification::Type::kHideUi);
        EXPECT_FALSE(advertisement->battery_notification->battery_infos.at(0)
                         .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(0).percentage,
            1);
        EXPECT_FALSE(advertisement->battery_notification->battery_infos.at(1)
                         .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(1).percentage,
            4);
        ASSERT_TRUE(advertisement->battery_notification->battery_infos.at(2)
                        .is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->battery_infos.at(2).percentage,
            15);
        latch.CountDown();
      });
  latch.Await();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
