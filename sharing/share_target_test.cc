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

#include "sharing/share_target.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "internal/network/url.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {
namespace {

struct ShareTargetToStringTestData {
  ShareTarget share_target;
  std::string expected_string_result;
};

std::vector<ShareTargetToStringTestData> GetTestData() {
  ShareTarget share_target1;
  ShareTarget share_target2{"test_name",
                            ::nearby::network::Url(),
                            ShareTargetType::kPhone,
                            /* is_incoming */ true,
                            "test_full_name",
                            /* is_known */ false,
                            "test_device_id",
                            true};
  share_target1.id = 1;
  share_target2.id = 2;

  static std::vector<ShareTargetToStringTestData>*
      kShareTargetToStringTestData =
          new std::vector<ShareTargetToStringTestData>({
              {share_target1,
               "ShareTarget<id: 1, type: 0, device_name: , is_known: 0, "
               "is_incoming: 0, for_self_share: 0, vendor_id: 0>"},
              {share_target2,
               "ShareTarget<id: 2, type: 1, device_name: test_name, full_name: "
               "test_full_name, image_url: ://:0, device_id: test_device_id, "
               "is_known: 0, is_incoming: 1, for_self_share: 1, vendor_id: 0>"},
          });

  return *kShareTargetToStringTestData;
}

using ShareTargetToStringTest =
    testing::TestWithParam<ShareTargetToStringTestData>;

TEST_P(ShareTargetToStringTest, ToStringResultMatches) {
  ShareTarget test_share_target = GetParam().share_target;
  EXPECT_EQ(GetParam().expected_string_result, test_share_target.ToString());
}

INSTANTIATE_TEST_SUITE_P(ShareTargetToStringTest, ShareTargetToStringTest,
                         testing::ValuesIn(GetTestData()));

}  // namespace
}  // namespace sharing
}  // namespace nearby
