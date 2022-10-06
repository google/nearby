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

#include "gtest/gtest.h"
#include "presence/data_types.h"
#include "presence/status.h"

namespace nearby {
namespace presence {
namespace {

TEST(PresenceClientTest, StartBroadcastWithDefaultConstructor) {
  Status broadcast_result = {Status::Value::kSuccess};
  BroadcastCallback broadcast_callback = {
      .start_broadcast_cb = [&](Status status) { broadcast_result = status; },
  };

  PresenceClient presence_client;
  presence_client.StartBroadcast({}, broadcast_callback);

  EXPECT_FALSE(broadcast_result.Ok());
}

TEST(PresenceClientTest, StartScanWithDefaultConstructor) {
  Status scan_result = {Status::Value::kSuccess};
  ScanCallback scan_callback = {
      .start_scan_cb = [&](Status status) { scan_result = status; },
  };

  PresenceClient presence_client;
  presence_client.StartScan({}, scan_callback);

  EXPECT_FALSE(scan_result.Ok());
}

}  // namespace
}  // namespace presence
}  // namespace nearby
