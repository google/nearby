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

#include "presence/presence_client.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"
#include "presence/data_types.h"
#include "presence/presence_service.h"
#include "presence/status.h"

namespace nearby {
namespace presence {
namespace {

using ::testing::status::StatusIs;

// Creates a PresenceClient and destroys PresenceService that was used to create
// it.
PresenceClient CreateDefunctPresenceClient() {
  PresenceService presence_service;
  return presence_service.CreatePresenceClient();
}

class PresenceClientTest : public testing::Test {
 protected:
  location::nearby::MediumEnvironment& env_{
      location::nearby::MediumEnvironment::Instance()};
};

TEST_F(PresenceClientTest, StartBroadcastWithDefaultConstructor) {
  env_.Start();
  Status broadcast_result = {Status::Value::kError};
  BroadcastCallback broadcast_callback = {
      .start_broadcast_cb = [&](Status status) { broadcast_result = status; },
  };

  PresenceService presence_service;
  PresenceClient presence_client = presence_service.CreatePresenceClient();
  auto unused = presence_client.StartBroadcast({}, broadcast_callback);

  EXPECT_FALSE(broadcast_result.Ok());
  env_.Stop();
}

TEST_F(PresenceClientTest, StartBroadcastFailsWhenPresenceServiceIsGone) {
  env_.Start();
  Status broadcast_result = {Status::Value::kError};
  BroadcastCallback broadcast_callback = {
      .start_broadcast_cb = [&](Status status) { broadcast_result = status; },
  };

  absl::StatusOr<BroadcastSessionId> session_id =
      CreateDefunctPresenceClient().StartBroadcast({}, broadcast_callback);

  EXPECT_THAT(session_id, StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_FALSE(broadcast_result.Ok());
  env_.Stop();
}

TEST_F(PresenceClientTest, StartScanWithDefaultConstructor) {
  env_.Start();
  ::location::nearby::Future<Status> scan_result;
  ScanCallback scan_callback = {
      .start_scan_cb = [&](Status status) { scan_result.Set(status); },
  };

  PresenceService presence_service;
  PresenceClient presence_client = presence_service.CreatePresenceClient();
  EXPECT_OK(presence_client.StartScan({}, scan_callback));

  EXPECT_TRUE(scan_result.Get().ok());
  EXPECT_TRUE(scan_result.Get().GetResult().Ok());
  env_.Stop();
}

TEST_F(PresenceClientTest, StartScanFailsWhenPresenceServiceIsGone) {
  env_.Start();
  Status scan_result = {Status::Value::kError};
  ScanCallback scan_callback = {
      .start_scan_cb = [&](Status status) { scan_result = status; },
  };

  absl::StatusOr<ScanSessionId> session_id =
      CreateDefunctPresenceClient().StartScan({}, scan_callback);

  EXPECT_THAT(session_id, StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_FALSE(scan_result.Ok());
  env_.Stop();
}

}  // namespace
}  // namespace presence
}  // namespace nearby
