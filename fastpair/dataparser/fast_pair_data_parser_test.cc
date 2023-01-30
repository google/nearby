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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "fastpair/crypto/fast_pair_encryption.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"

namespace nearby {
namespace fastpair {
namespace {

constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);
constexpr absl::string_view kModelId("aabbcc");
constexpr absl::string_view kInvalidModelId("aabb");
constexpr std::array<uint8_t, kAesBlockByteSize> kAeskeyarray = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

TEST(FastPairDataParserTest, GetHexModelIdFromServiceDataUnsucessfully) {
  const std::vector<uint8_t> service_data =
      FastPairServiceDataCreator::Builder()
          .SetModelId(std::string(kInvalidModelId))
          .Build()
          ->CreateServiceData();
  absl::Notification notification;
  GetHexModelIdFromServiceDataCallback callback;
  callback.on_retrieved_cb =
      [&notification](std::optional<absl::string_view> model_id) {
        EXPECT_EQ(model_id, std::nullopt);
        notification.Notify();
      };

  FastPairDataParser::GetHexModelIdFromServiceData(service_data,
                                                   std::move(callback));

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

TEST(FastPairDataParserTest, GetHexModelIdFromServiceDataSuccessfully) {
  const std::vector<uint8_t> service_data =
      FastPairServiceDataCreator::Builder()
          .SetModelId(std::string(kModelId))
          .Build()
          ->CreateServiceData();
  absl::Notification notification;
  GetHexModelIdFromServiceDataCallback callback;
  callback.on_retrieved_cb =
      [&notification](std::optional<absl::string_view> model_id) {
        EXPECT_EQ(model_id, kModelId);
        notification.Notify();
      };

  FastPairDataParser::GetHexModelIdFromServiceData(service_data,
                                                   std::move(callback));

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptResponseCallback callback;
  callback.on_decrypted_cb =
      [&notification](std::optional<DecryptedResponse> response) {
        EXPECT_FALSE(response.has_value());
        notification.Notify();
      };

  FastPairDataParser::ParseDecryptedResponse(kAesKeyBytes, encrypted_bytes,
                                             std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptResponseCallback callback;
  callback.on_decrypted_cb =
      [&notification](std::optional<DecryptedResponse> response) {
        EXPECT_FALSE(response.has_value());
        notification.Notify();
      };

  FastPairDataParser::ParseDecryptedResponse(kAesKeyBytes, encrypted_bytes,
                                             std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptResponseCallback callback;
  callback.on_decrypted_cb = [&notification, &kAddressBytes, &kSalt](
                                 std::optional<DecryptedResponse> response) {
    EXPECT_TRUE(response.has_value());
    EXPECT_EQ(response.value().message_type,
              FastPairMessageType::kKeyBasedPairingResponse);
    EXPECT_EQ(response.value().address_bytes, kAddressBytes);
    EXPECT_EQ(response.value().salt, kSalt);
    notification.Notify();
  };

  FastPairDataParser::ParseDecryptedResponse(kAesKeyBytes, encrypted_bytes,
                                             std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptPasskeyCallback callback;
  callback.on_decrypted_cb =
      [&notification](std::optional<DecryptedPasskey> decrypted_passkey) {
        EXPECT_FALSE(decrypted_passkey.has_value());
        notification.Notify();
      };

  FastPairDataParser::ParseDecryptedPasskey(kAesKeyBytes, encrypted_bytes,
                                            std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptPasskeyCallback callback;
  callback.on_decrypted_cb =
      [&notification](std::optional<DecryptedPasskey> decrypted_passkey) {
        EXPECT_FALSE(decrypted_passkey.has_value());
        notification.Notify();
      };

  FastPairDataParser::ParseDecryptedPasskey(kAesKeyBytes, encrypted_bytes,
                                            std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptPasskeyCallback callback;
  callback.on_decrypted_cb =
      [&notification, &kPasskey,
       &kSalt](std::optional<DecryptedPasskey> decrypted_passkey) {
        EXPECT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey.value().message_type,
                  FastPairMessageType::kSeekersPasskey);
        EXPECT_EQ(decrypted_passkey.value().passkey, kPasskey);
        EXPECT_EQ(decrypted_passkey.value().salt, kSalt);
        notification.Notify();
      };

  FastPairDataParser::ParseDecryptedPasskey(kAesKeyBytes, encrypted_bytes,
                                            std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

  absl::Notification notification;
  ParseDecryptPasskeyCallback callback;
  callback.on_decrypted_cb =
      [&notification, &kPasskey,
       &kSalt](std::optional<DecryptedPasskey> decrypted_passkey) {
        EXPECT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey.value().message_type,
                  FastPairMessageType::kProvidersPasskey);
        EXPECT_EQ(decrypted_passkey.value().passkey, kPasskey);
        EXPECT_EQ(decrypted_passkey.value().salt, kSalt);
        notification.Notify();
      };

  FastPairDataParser::ParseDecryptedPasskey(kAesKeyBytes, encrypted_bytes,
                                            std::move(callback));
  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
