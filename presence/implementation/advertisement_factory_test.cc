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
  MOCK_METHOD(absl::StatusOr<std::string>, GetBaseEncryptedMetadataKey,
              (const IdentityType& identity), (override));
  MOCK_METHOD(absl::StatusOr<std::string>, EncryptDataElements,
              (const IdentityType& identity, absl::string_view salt,
               absl::string_view data_elements),
              (override));
};

TEST(AdvertisementFactory, CreateAdvertisementFromPrivateIdentity) {
  std::string salt = "AB";
  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = IdentityType::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kActionFieldType,
                             action::kActiveUnlockAction);
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt(salt)
                               .SetTxPower(5)
                               .SetAction(action));
  EXPECT_CALL(credential_manager, GetBaseEncryptedMetadataKey(kIdentity))
      .WillOnce(Return(absl::HexStringToBytes("1011121314151617181920212223")));
  EXPECT_CALL(credential_manager,
              EncryptDataElements(kIdentity, salt,
                                  absl::HexStringToBytes("1505260080")))
      .WillOnce(Return(absl::HexStringToBytes("5051525354")));

  AdvertisementFactory factory(&credential_manager);
  absl::StatusOr<BleAdvertisementData> result =
      factory.CreateAdvertisement(request);

  EXPECT_OK(result);
  auto service_data = result->service_data;
  EXPECT_EQ(service_data.size(), 1);
  for (auto i : service_data) {
    EXPECT_EQ(i.first.Get16BitAsString(), "FCF1");
    auto advertisement = absl::BytesToHexString(i.second.AsStringView());
    EXPECT_EQ(advertisement,
              "00204142e110111213141516171819202122235051525354");
  }
}

TEST(AdvertisementFactory, CreateAdvertisementFailsWhenCredentialManagerFails) {
  NiceMock<MockCredentialManager> credential_manager;
  constexpr IdentityType kIdentity = internal::IDENTITY_TYPE_PRIVATE;
  std::vector<DataElement> data_elements;
  data_elements.emplace_back(DataElement::kActionFieldType,
                             action::kActiveUnlockAction);
  Action action = ActionFactory::CreateAction(data_elements);
  BaseBroadcastRequest request =
      BaseBroadcastRequest(BasePresenceRequestBuilder(kIdentity)
                               .SetSalt("AB")
                               .SetTxPower(5)
                               .SetAction(action));
  EXPECT_CALL(credential_manager, GetBaseEncryptedMetadataKey(kIdentity))
      .WillOnce(Return(absl::UnimplementedError(
          "GetBaseEncryptedMetadataKey not implemented")));

  AdvertisementFactory factory(&credential_manager);
  EXPECT_THAT(factory.CreateAdvertisement(request),
              StatusIs(absl::StatusCode::kUnimplemented));
}
}  // namespace
}  // namespace presence
}  // namespace nearby
