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
#include "presence/status.h"

namespace nearby {
namespace presence {
namespace {

TEST(PresenceClientTest, DefaultConstructorWorks) {
  PresenceClient presence_client;
  Status result = {Status::Value::kSuccess};
  ResultCallback callback = {
      .result_cb = [&](Status status) { result = status; },
  };

  presence_client.StartBroadcast({}, callback);

  EXPECT_FALSE(result.Ok());
}

}  // namespace
}  // namespace presence
}  // namespace nearby
