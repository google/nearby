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

#include "presence/implementation/advertisement_factory.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "internal/platform/byte_array.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/implementation/action_factory.h"
#include "presence/implementation/mediums/advertisement_data.h"

namespace nearby {
namespace presence {

namespace {

using ::nearby::ByteArray;  // NOLINT
using ::nearby::internal::IdentityType;
using ::nearby::internal::LocalCredential;  // NOLINT
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::status::StatusIs;

#if USE_RUST_LDT == 1
LocalCredential CreateLocalCredential(IdentityType identity_type) {
  // Values copied from LDT tests
  ByteArray seed({204, 219, 36, 137, 233, 252, 172, 66, 179, 147, 72,
                  184, 148, 30, 209, 154, 29,  54,  14, 117, 224, 152,
                  200, 193, 94, 107, 28,  194, 182, 32, 205, 57});
  ByteArray metadata_key(
      {205, 104, 63, 225, 161, 209, 248, 70, 84, 61, 10, 19, 212, 174});

  LocalCredential private_credential;
  private_credential.set_identity_type(identity_type);
  private_credential.set_key_seed(seed.AsStringView());
  private_credential.set_metadata_encryption_key(metadata_key.AsStringView());
  return private_credential;
}

TEST(AdvertisementFactory, CreateAdvertisementFromPrivateIdentity) {
  std::string account_name = "Test account";
  std::string salt = "AB";
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetAccountName(account_name)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));

  absl::StatusOr<AdvertisementData> result =
      AdvertisementFactory().CreateAdvertisement(
          request, CreateLocalCredential(kIdentity));

  ASSERT_OK(result);
  EXPECT_FALSE(result->is_extended_advertisement);
  EXPECT_EQ(absl::BytesToHexString(result->content),
            "00414142ceb073b0e34f58d7dc6dea370783ac943fa5");
}

TEST(AdvertisementFactory, CreateAdvertisementFromTrustedIdentity) {
  std::string account_name = "Test account";
  std::string salt = "AB";
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_TRUSTED;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  data_elements.emplace_back(DataElement(ActionBit::kFitCastAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetAccountName(account_name)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));

  absl::StatusOr<AdvertisementData> result =
      AdvertisementFactory().CreateAdvertisement(
          request, CreateLocalCredential(kIdentity));

  ASSERT_OK(result);
  EXPECT_FALSE(result->is_extended_advertisement);
  EXPECT_EQ(absl::BytesToHexString(result->content),
            "00424142253536ac63191a96894d95f0ffa38b57cf9b");
}

TEST(AdvertisementFactory, CreateAdvertisementFromProvisionedIdentity) {
  std::string account_name = "Test account";
  std::string salt = "AB";
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PROVISIONED;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  data_elements.emplace_back(DataElement(ActionBit::kFitCastAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetAccountName(account_name)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));

  absl::StatusOr<AdvertisementData> result =
      AdvertisementFactory().CreateAdvertisement(
          request, CreateLocalCredential(kIdentity));

  ASSERT_OK(result);
  EXPECT_FALSE(result->is_extended_advertisement);
  EXPECT_EQ(absl::BytesToHexString(result->content),
            "00444142253536ac63191a96894d95f0ffa38b57cf9b");
}
#endif /*USE_RUST_LDT*/

TEST(AdvertisementFactory, CreateAdvertisementFromPublicIdentity) {
  std::string salt = "AB";
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PUBLIC;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));

  absl::StatusOr<AdvertisementData> result =
      AdvertisementFactory().CreateAdvertisement(request);

  ASSERT_OK(result);
  EXPECT_FALSE(result->is_extended_advertisement);
  EXPECT_EQ(absl::BytesToHexString(result->content), "000320414236050080");
}

TEST(AdvertisementFactory, CreateAdvertisementFailsWhenSaltIsTooShort) {
  std::string salt = "AB";
  constexpr IdentityType kIdentity = internal::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));
  // Override the salt with invalid value
  request.salt = "C";

  EXPECT_THAT(AdvertisementFactory().CreateAdvertisement(request),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
