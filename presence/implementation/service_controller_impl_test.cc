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

#include "presence/implementation/service_controller_impl.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/future.h"
#include "internal/platform/medium_environment.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {
namespace {

using FeatureFlags = ::location::nearby::FeatureFlags::Flags;
using internal::IdentityType;
using ::location::nearby::MediumEnvironment;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{},
};

constexpr absl::string_view kAccountName = "Test account";
constexpr int8_t kTxPower = 30;

BroadcastRequest CreateBroadcastRequest(IdentityType identity) {
  PresenceBroadcast::BroadcastSection section = {
      .identity = identity,
      .extended_properties = {DataElement(
          DataElement(ActionBit::kActiveUnlockAction))},
      .account_name = std::string(kAccountName)};
  PresenceBroadcast presence_request = {.sections = {section}};
  BroadcastRequest request = {.tx_power = kTxPower,
                              .variant = presence_request};
  return request;
}

class MediumEnvironmentStarter {
 public:
  MediumEnvironmentStarter() { MediumEnvironment::Instance().Start(); }
  ~MediumEnvironmentStarter() { MediumEnvironment::Instance().Stop(); }
};

class ServiceControllerImplTest : public testing::TestWithParam<FeatureFlags> {
 protected:
  // The medium environment must be initialized (started) before the service
  // controller.
  MediumEnvironmentStarter env_;
  ServiceControllerImpl service_controller_;
  location::nearby::Future<Status> start_broadcast_status_;
  BroadcastCallback broadcast_callback_{
      .start_broadcast_cb = [this](Status status) {
        start_broadcast_status_.Set(status);
      }};
};

INSTANTIATE_TEST_SUITE_P(ParametrisedServiceControllerImplTest,
                         ServiceControllerImplTest,
                         testing::ValuesIn(kTestCases));

TEST_P(ServiceControllerImplTest, StartBroadcastPublicIdentity) {
  std::unique_ptr<BroadcastSession> session =
      service_controller_.StartBroadcast(
          CreateBroadcastRequest(internal::IDENTITY_TYPE_PUBLIC),
          broadcast_callback_);

  EXPECT_TRUE(session);
  EXPECT_TRUE(start_broadcast_status_.Get().ok());
  EXPECT_EQ(start_broadcast_status_.Get().GetResult(),
            Status{Status::Value::kSuccess});
  EXPECT_TRUE(MediumEnvironment::Instance()
                  .GetBleV2MediumStatus(
                      *service_controller_.GetMediums().GetBle().GetImpl())
                  ->is_advertising);
}

TEST_P(ServiceControllerImplTest, StartAndStopBroadcast) {
  std::unique_ptr<BroadcastSession> session =
      service_controller_.StartBroadcast(
          CreateBroadcastRequest(internal::IDENTITY_TYPE_PUBLIC),
          broadcast_callback_);

  ASSERT_TRUE(session);
  EXPECT_EQ(session->stop_broadcast_callback(),
            Status{Status::Value::kSuccess});
  MediumEnvironment::Instance().Sync();
  EXPECT_FALSE(MediumEnvironment::Instance()
                   .GetBleV2MediumStatus(
                       *service_controller_.GetMediums().GetBle().GetImpl())
                   ->is_advertising);
}

TEST_P(ServiceControllerImplTest, StartBroadcastInvalidRequestFails) {
  std::unique_ptr<BroadcastSession> session =
      service_controller_.StartBroadcast(BroadcastRequest{},
                                         broadcast_callback_);

  EXPECT_FALSE(session);
  EXPECT_TRUE(start_broadcast_status_.Get().ok());
  EXPECT_EQ(start_broadcast_status_.Get().GetResult(),
            Status{Status::Value::kError});
}

TEST_P(ServiceControllerImplTest, StartBroadcastPrivateIdentityFails) {
  // TODO(b/256249404): Support private identity.
  std::unique_ptr<BroadcastSession> session =
      service_controller_.StartBroadcast(
          CreateBroadcastRequest(internal::IDENTITY_TYPE_PRIVATE),
          broadcast_callback_);

  EXPECT_FALSE(session);
  EXPECT_TRUE(start_broadcast_status_.Get().ok());
  EXPECT_EQ(start_broadcast_status_.Get().GetResult(),
            Status{Status::Value::kError});
}

}  // namespace
}  // namespace presence
}  // namespace nearby
