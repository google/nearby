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
#include "absl/status/status.h"
#include "internal/platform/medium_environment.h"
#include "presence/data_types.h"
#include "presence/presence_service.h"

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
  nearby::MediumEnvironment& env_{nearby::MediumEnvironment::Instance()};
};

TEST_F(PresenceClientTest, StartBroadcastWithDefaultConstructor) {
  env_.Start();
  absl::Status broadcast_result;

  PresenceService presence_service;
  PresenceClient presence_client = presence_service.CreatePresenceClient();
  auto unused = presence_client.StartBroadcast(
      {}, {
              .start_broadcast_cb =
                  [&](absl::Status status) { broadcast_result = status; },
          });

  EXPECT_THAT(broadcast_result, StatusIs(absl::StatusCode::kInvalidArgument));
  env_.Stop();
}

TEST_F(PresenceClientTest, StartBroadcastFailsWhenPresenceServiceIsGone) {
  env_.Start();
  absl::Status broadcast_result = absl::UnknownError("");

  absl::StatusOr<BroadcastSessionId> session_id =
      CreateDefunctPresenceClient().StartBroadcast(
          {}, {
                  .start_broadcast_cb =
                      [&](absl::Status status) { broadcast_result = status; },
              });

  EXPECT_THAT(session_id, StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(broadcast_result, StatusIs(absl::StatusCode::kUnknown));
  env_.Stop();
}

TEST_F(PresenceClientTest, StartScanWithDefaultConstructor) {
  env_.Start();
  ::nearby::Future<absl::Status> scan_result;
  ScanCallback scan_callback = {
      .start_scan_cb = [&](absl::Status status) { scan_result.Set(status); },
  };

  PresenceService presence_service;
  PresenceClient presence_client = presence_service.CreatePresenceClient();
  EXPECT_OK(presence_client.StartScan({}, std::move(scan_callback)));

  EXPECT_TRUE(scan_result.Get().ok());
  EXPECT_OK(scan_result.Get().GetResult());
  env_.Stop();
}

TEST_F(PresenceClientTest, StartScanFailsWhenPresenceServiceIsGone) {
  env_.Start();
  absl::Status scan_result = absl::UnknownError("");

  absl::StatusOr<ScanSessionId> session_id =
      CreateDefunctPresenceClient().StartScan(
          {}, {
                  .start_scan_cb =
                      [&](absl::Status status) { scan_result = status; },
              });

  EXPECT_THAT(session_id, StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(scan_result, StatusIs(absl::StatusCode::kUnknown));
  env_.Stop();
}

}  // namespace
}  // namespace presence
}  // namespace nearby
