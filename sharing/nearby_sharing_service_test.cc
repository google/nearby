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

#include "sharing/nearby_sharing_service.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::NearbySharingService;
using StatusCodes = NearbySharingService::StatusCodes;

struct StatusCodeToStringData {
  StatusCodes status_code;
  std::string expected_string_result;
};

std::vector<StatusCodeToStringData> GetTestData() {
  static std::vector<StatusCodeToStringData>* kStatusCodeToStringData =
      new std::vector<StatusCodeToStringData>({
          {StatusCodes::kOk, "kOk"},
          {StatusCodes::kError, "kError"},
          {StatusCodes::kOutOfOrderApiCall, "kOutOfOrderApiCall"},
          {StatusCodes::kStatusAlreadyStopped, "kStatusAlreadyStopped"},
          {StatusCodes::kTransferAlreadyInProgress,
           "kTransferAlreadyInProgress"},
          {StatusCodes::kNoAvailableConnectionMedium,
           "kNoAvailableConnectionMedium"},
          {StatusCodes::kIrrecoverableHardwareError,
           "kIrrecoverableHardwareError"},
          {StatusCodes::kInvalidArgument, "kInvalidArgument"},
          // If entries are added, kMaxValue and
          // NearbySharingService::StatusCodeToString should be updated.
          {StatusCodes::kMaxValue, "kInvalidArgument"},
      });

  return *kStatusCodeToStringData;
}

using StatusCodeToString = testing::TestWithParam<StatusCodeToStringData>;

TEST_P(StatusCodeToString, ToStringResultMatches) {
  EXPECT_EQ(GetParam().expected_string_result,
            NearbySharingService::StatusCodeToString(GetParam().status_code));
}

INSTANTIATE_TEST_CASE_P(StatusCodeToString, StatusCodeToString,
                        testing::ValuesIn(GetTestData()));

}  // namespace
}  // namespace sharing
}  // namespace nearby
