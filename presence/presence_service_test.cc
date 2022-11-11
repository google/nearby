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

#include "presence/presence_service.h"

#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"
#include "presence/presence_client.h"
#include "presence/status.h"

namespace nearby {
namespace presence {
namespace {

class PresenceServiceTest : public testing::Test {
 protected:
  location::nearby::MediumEnvironment& env_{
      location::nearby::MediumEnvironment::Instance()};
};

TEST_F(PresenceServiceTest, DefaultConstructorWorks) {
  PresenceService presence_service;
}

TEST_F(PresenceServiceTest, StartThenStopScan) {
  env_.Start();
  Status scan_result = {Status::Value::kSuccess};
  ScanCallback scan_callback = {
      .start_scan_cb = [&](Status status) { scan_result = status; },
  };
  PresenceService presence_service;
  PresenceClient client = presence_service.CreatePresenceClient();

  auto scan_session = client.StartScan(
      {}, {
              .start_scan_cb = [&](Status status) { scan_result = status; },
          });
  auto scan_session_with_default_params =
      client.StartScan(ScanRequest(), ScanCallback());

  EXPECT_NE(scan_session, std::nullopt);
  EXPECT_NE(scan_session_with_default_params, std::nullopt);

  Status stop_scan_session_status = scan_session->StopScan();
  Status scan_session_with_default_params_status =
      scan_session_with_default_params->StopScan();

  EXPECT_TRUE(stop_scan_session_status.Ok());
  EXPECT_TRUE(scan_session_with_default_params_status.Ok());
  env_.Stop();
}

}  // namespace
}  // namespace presence
}  // namespace nearby
