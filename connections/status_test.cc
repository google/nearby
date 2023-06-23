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

#include "connections/status.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace connections {

TEST(StatusTest, DefaultIsError) {
  Status status;
  EXPECT_FALSE(status.Ok());
  EXPECT_EQ(status, Status{Status::kError});
}

TEST(StatusTest, DefaultEquals) {
  Status status1;
  Status status2;
  EXPECT_EQ(status1, status2);
}

TEST(StatusTest, ExplicitInitEquals) {
  Status status1 = {Status::kSuccess};
  Status status2 = {Status::kSuccess};
  EXPECT_EQ(status1, status2);
  EXPECT_TRUE(status1.Ok());
}

TEST(StatusTest, ExplicitInitNotEquals) {
  Status status1 = {Status::kSuccess};
  Status status2 = {Status::kAlreadyAdvertising};
  EXPECT_NE(status1, status2);
}

TEST(StatusTest, CopyInitEquals) {
  Status status1 = {Status::kAlreadyAdvertising};
  Status status2 = {status1};

  EXPECT_EQ(status1, status2);
}

struct StatusToStringData {
  Status status;
  std::string expected_string_result;
};

std::vector<StatusToStringData> GetTestData() {
  return {
      {Status{.value = Status::kSuccess}, "kSuccess"},
      {Status{.value = Status::kError}, "kError"},
      {Status{.value = Status::kOutOfOrderApiCall}, "kOutOfOrderApiCall"},
      {Status{.value = Status::kAlreadyHaveActiveStrategy},
       "kAlreadyHaveActiveStrategy"},
      {Status{.value = Status::kAlreadyAdvertising}, "kAlreadyAdvertising"},
      {Status{.value = Status::kAlreadyDiscovering}, "kAlreadyDiscovering"},
      {Status{.value = Status::kAlreadyListening}, "kAlreadyListening"},
      {Status{.value = Status::kEndpointIoError}, "kEndpointIoError"},
      {Status{.value = Status::kEndpointUnknown}, "kEndpointUnknown"},
      {Status{.value = Status::kConnectionRejected}, "kConnectionRejected"},
      {Status{.value = Status::kAlreadyConnectedToEndpoint},
       "kAlreadyConnectedToEndpoint"},
      {Status{.value = Status::kNotConnectedToEndpoint},
       "kNotConnectedToEndpoint"},
      {Status{.value = Status::kBluetoothError}, "kBluetoothError"},
      {Status{.value = Status::kBleError}, "kBleError"},
      {Status{.value = Status::kWifiLanError}, "kWifiLanError"},
      {Status{.value = Status::kPayloadUnknown}, "kPayloadUnknown"},
  };
}

using StatusToString = testing::TestWithParam<StatusToStringData>;

TEST_P(StatusToString, ToStringResultMatches) {
  EXPECT_EQ(GetParam().expected_string_result, GetParam().status.ToString());
}

INSTANTIATE_TEST_CASE_P(StatusToString, StatusToString,
                        testing::ValuesIn(GetTestData()));

}  // namespace connections
}  // namespace nearby
