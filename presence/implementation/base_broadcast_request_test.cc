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

#include "presence/implementation/base_broadcast_request.h"

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "internal/proto/credential.pb.h"
#include "presence/broadcast_request.h"
#include "presence/data_element.h"
#include "presence/power_mode.h"

namespace nearby {
namespace presence {
namespace {

using ::nearby::internal::IdentityType;
using ::testing::status::StatusIs;

TEST(BroadcastRequestTest, CreateBasePresenceRequest) {
  nearby::internal::IdentityType identity;
  constexpr int8_t kTxPower = -13;

  BaseBroadcastRequest request = BaseBroadcastRequest(
      BasePresenceRequestBuilder(identity).SetTxPower(kTxPower).SetPowerMode(
          PowerMode::kBalanced));
  EXPECT_EQ(request.salt.size(), 2);
  EXPECT_EQ(request.tx_power, kTxPower);
  EXPECT_EQ(request.power_mode, PowerMode::kBalanced);
}

TEST(BroadcastRequestTest, CreateFromPresenceRequest) {
  constexpr int8_t kTxPower = 30;
  std::string account_name = "Test account";
  std::string manager_app_id = "Manager app id";
  BroadcastRequest::BroadcastSection section = {
      .identity = internal::IDENTITY_TYPE_PUBLIC,
      .extended_properties = {DataElement(
          DataElement(ActionBit::kActiveUnlockAction))},
      .account_name = account_name,
      .manager_app_id = manager_app_id};
  BroadcastRequest input = {
      .tx_power = kTxPower,
      .sections = {section},
  };

  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(input);

  ASSERT_OK(request);
  EXPECT_THAT(request->tx_power, kTxPower);
  EXPECT_THAT(request->credential_selector.identity_type,
              IdentityType::IDENTITY_TYPE_PUBLIC);
  EXPECT_THAT(request->credential_selector.account_name, account_name);
  EXPECT_THAT(request->credential_selector.manager_app_id, manager_app_id);
}

TEST(BroadcastRequestTest, CreateFromEmptyPresenceRequestFails) {
  BroadcastRequest empty = {
  };

  EXPECT_THAT(BaseBroadcastRequest::Create(empty),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace presence
}  // namespace nearby
