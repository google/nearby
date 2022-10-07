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
#include "presence/data_element.h"
#include "presence/implementation/action_factory.h"
#include "presence/implementation/credential_manager_impl.h"

namespace nearby {
namespace presence {

namespace {

using ::nearby::internal::IdentityType;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::status::StatusIs;

class MockCredentialManager : public CredentialManagerImpl {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, EncryptDataElements,
              (IdentityType identity, absl::string_view salt,
               absl::string_view data_elements),
              (override));
};

TEST(AdvertisementFactory, CreateAdvertisementFromPrivateIdentity) {
  std::string salt = "AB";
  std::string metadata_key =
      absl::HexStringToBytes("1011121314151617181920212223");
  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));
  EXPECT_CALL(
      credential_manager,
      EncryptDataElements(kIdentity, salt, absl::HexStringToBytes("36050080")))
      .WillOnce(Return(metadata_key + absl::HexStringToBytes("50515253")));

  AdvertisementFactory factory(&credential_manager);
  absl::StatusOr<BleAdvertisementData> result =
      factory.CreateAdvertisement(request);

  ASSERT_OK(result);
  auto service_data = result->service_data;
  ASSERT_EQ(service_data.size(), 1);
  for (auto i : service_data) {
    EXPECT_EQ(i.first.Get16BitAsString(), "FCF1");
    auto advertisement = absl::BytesToHexString(i.second.AsStringView());
    EXPECT_EQ(advertisement, "00414142101112131415161718192021222350515253");
  }
}

TEST(AdvertisementFactory, CreateAdvertisementFromPublicIdentity) {
  std::string salt = "AB";
  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PUBLIC;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));

  AdvertisementFactory factory(&credential_manager);
  absl::StatusOr<BleAdvertisementData> result =
      factory.CreateAdvertisement(request);

  ASSERT_OK(result);
  auto service_data = result->service_data;
  EXPECT_EQ(service_data.size(), 1);
  for (auto i : service_data) {
    EXPECT_EQ(i.first.Get16BitAsString(), "FCF1");
    auto advertisement = absl::BytesToHexString(i.second.AsStringView());
    EXPECT_EQ(advertisement, "000320414236050080");
  }
}

TEST(AdvertisementFactory, CreateAdvertisementFailsWhenEncryptionFails) {
  std::string salt = "AB";
  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = internal::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));
  EXPECT_CALL(
      credential_manager,
      EncryptDataElements(kIdentity, salt, absl::HexStringToBytes("36050080")))
      .WillOnce(Return(absl::OutOfRangeError("failed")));

  AdvertisementFactory factory(&credential_manager);
  EXPECT_THAT(factory.CreateAdvertisement(request),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementFactory,
     CreateAdvertisementFailsWhenEncryptionReturnsTooMuchData) {
  std::string salt = "AB";
  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = internal::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));
  EXPECT_CALL(
      credential_manager,
      EncryptDataElements(kIdentity, salt, absl::HexStringToBytes("36050080")))
      .WillOnce(Return(absl::HexStringToBytes(
          "deaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddead"
          "deaddeaddeaddeaddeaddeaddead")));

  AdvertisementFactory factory(&credential_manager);
  EXPECT_THAT(factory.CreateAdvertisement(request),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AdvertisementFactory,
     CreateAdvertisementFailsWhenEncryptionReturnsTooLittleData) {
  std::string salt = "AB";
  std::string metadata_key =
      absl::HexStringToBytes("1011121314151617181920212223");

  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = internal::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement(ActionBit::kActiveUnlockAction));
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));
  EXPECT_CALL(
      credential_manager,
      EncryptDataElements(kIdentity, salt, absl::HexStringToBytes("36050080")))
      .WillOnce(Return(metadata_key));

  AdvertisementFactory factory(&credential_manager);
  EXPECT_THAT(factory.CreateAdvertisement(request),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(AdvertisementFactory, CreateAdvertisementFailsWhenSaltIsTooShort) {
  std::string salt = "AB";
  NiceMock<MockCredentialManager> credential_manager;
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

  AdvertisementFactory factory(&credential_manager);
  EXPECT_THAT(factory.CreateAdvertisement(request),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
