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

#include <deque>
#include <iostream>
#include <vector>

#include "fakes.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nearby.h"
#include "nearby_advert.h"
#include "nearby_event.h"
#include "nearby_fp_library.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_os.h"
#include "nearby_spot.h"

using ::testing::ElementsAreArray;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"

#pragma GCC diagnostic pop

constexpr uint8_t kFpAdvertisement[NON_DISCOVERABLE_ADV_SIZE_BYTES] = {1};
#if NEARBY_FP_ENABLE_SPOT
constexpr uint8_t kSpotAdvertisement[NEARBY_SPOT_ADVERTISEMENT_SIZE] = {2};
#endif /* NEARBY_FP_ENABLE_SPOT */

class AdvertTest : public ::testing::Test {
 protected:
  void SetUp() override;
};

void AdvertTest::SetUp() {
  nearby_platform_OsInit(NULL);
  nearby_platform_BleInit(NULL);
  nearby_AdvertInit();
}

TEST_F(AdvertTest, SetFpAdvertisement_AdvertisesTimerOff) {
  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(kFpAdvertisement,
                                                       sizeof(kFpAdvertisement),
                                                       kNoLargerThan100ms));

  ASSERT_THAT(kFpAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, ChangeFpAdvertisement_AdvertisesTimerOff) {
  constexpr uint8_t kAdvertisement[NON_DISCOVERABLE_ADV_SIZE_BYTES] = {3};
  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(kFpAdvertisement,
                                                       sizeof(kFpAdvertisement),
                                                       kNoLargerThan100ms));

  ASSERT_EQ(kNearbyStatusOK,
            nearby_SetFpAdvertisement(kAdvertisement, sizeof(kAdvertisement),
                                      kNoLargerThan100ms));

  ASSERT_THAT(kAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, RemoveFpAdvertisement_NoAdvertisementTimerOff) {
  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(kFpAdvertisement,
                                                       sizeof(kFpAdvertisement),
                                                       kNoLargerThan100ms));

  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(NULL, 0, kDisabled));

  ASSERT_EQ(0, nearby_test_fakes_GetAdvertisement().size());
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

#if NEARBY_FP_ENABLE_SPOT
TEST_F(AdvertTest, SetSpotAdvertisement_AdvertisesTimerOff) {
  ASSERT_EQ(kNearbyStatusOK,
            nearby_SetSpotAdvertisement(0, kSpotAdvertisement,
                                        sizeof(kSpotAdvertisement)));

  ASSERT_THAT(kSpotAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, ChangeSpotAdvertisement_AdvertisesTimerOff) {
  constexpr uint8_t kAdvertisement[NON_DISCOVERABLE_ADV_SIZE_BYTES] = {3};
  ASSERT_EQ(kNearbyStatusOK,
            nearby_SetSpotAdvertisement(0, kSpotAdvertisement,
                                        sizeof(kSpotAdvertisement)));

  ASSERT_EQ(kNearbyStatusOK, nearby_SetSpotAdvertisement(0,
                                 kAdvertisement, sizeof(kAdvertisement)));

  ASSERT_THAT(kAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, RemoveSpotAdvertisement_NoAdvertisementTimerOff) {
  ASSERT_EQ(kNearbyStatusOK,
            nearby_SetSpotAdvertisement(0, kSpotAdvertisement,
                                        sizeof(kSpotAdvertisement)));

  ASSERT_EQ(kNearbyStatusOK, nearby_SetSpotAdvertisement(0, NULL, 0));

  ASSERT_EQ(0, nearby_test_fakes_GetSpotAdvertisement().size());
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

#ifndef NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS

TEST_F(AdvertTest, SetFpThenSpot_RotatesOnTimer) {
  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(kFpAdvertisement,
                                                       sizeof(kFpAdvertisement),
                                                       kNoLargerThan100ms));
  ASSERT_EQ(kNearbyStatusOK,
            nearby_SetSpotAdvertisement(kSpotAdvertisement,
                                        sizeof(kSpotAdvertisement)));

  ASSERT_THAT(kSpotAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  ASSERT_THAT(kFpAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  ASSERT_THAT(kSpotAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
}

TEST_F(AdvertTest, SetSpotThenFp_RotatesOnTimer) {
  ASSERT_EQ(kNearbyStatusOK,
            nearby_SetSpotAdvertisement(kSpotAdvertisement,
                                        sizeof(kSpotAdvertisement)));
  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(kFpAdvertisement,
                                                       sizeof(kFpAdvertisement),
                                                       kNoLargerThan100ms));

  ASSERT_THAT(kFpAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  ASSERT_THAT(kSpotAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  ASSERT_THAT(kFpAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
}

TEST_F(AdvertTest, RemoveFp_SpotActive_RemovesTimer) {
  nearby_SetFpAdvertisement(kFpAdvertisement, sizeof(kFpAdvertisement),
                            kNoLargerThan100ms);
  nearby_SetSpotAdvertisement(kSpotAdvertisement, sizeof(kSpotAdvertisement));

  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(NULL, 0, kDisabled));

  ASSERT_THAT(kSpotAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, RemoveFp_FpActive_SwitchesToSpotRemovesTimer) {
  nearby_SetSpotAdvertisement(kSpotAdvertisement, sizeof(kSpotAdvertisement));
  nearby_SetFpAdvertisement(kFpAdvertisement, sizeof(kFpAdvertisement),
                            kNoLargerThan100ms);

  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(NULL, 0, kDisabled));

  ASSERT_THAT(kSpotAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, RemoveSpot_FpActive_RemovesTimer) {
  nearby_SetSpotAdvertisement(kSpotAdvertisement, sizeof(kSpotAdvertisement));
  nearby_SetFpAdvertisement(kFpAdvertisement, sizeof(kFpAdvertisement),
                            kNoLargerThan100ms);

  ASSERT_EQ(kNearbyStatusOK, nearby_SetSpotAdvertisement(NULL, 0));

  ASSERT_THAT(kFpAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, RemoveSpot_SpotActive_SwitchesToFpRemovesTimer) {
  nearby_SetFpAdvertisement(kFpAdvertisement, sizeof(kFpAdvertisement),
                            kNoLargerThan100ms);
  nearby_SetSpotAdvertisement(kSpotAdvertisement, sizeof(kSpotAdvertisement));

  ASSERT_EQ(kNearbyStatusOK, nearby_SetSpotAdvertisement(NULL, 0));

  ASSERT_THAT(kFpAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetAdvertisement()));
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

TEST_F(AdvertTest, RemoveBoth_DisablesAdvertisementRemovesTimer) {
  nearby_SetSpotAdvertisement(kSpotAdvertisement, sizeof(kSpotAdvertisement));
  nearby_SetFpAdvertisement(kFpAdvertisement, sizeof(kFpAdvertisement),
                            kNoLargerThan100ms);

  ASSERT_EQ(kNearbyStatusOK, nearby_SetSpotAdvertisement(NULL, 0));
  ASSERT_EQ(kNearbyStatusOK, nearby_SetFpAdvertisement(NULL, 0, kDisabled));

  ASSERT_EQ(0, nearby_test_fakes_GetAdvertisement().size());
  ASSERT_EQ(0, nearby_test_fakes_GetNextTimerMs());
}

#endif /* NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS */
#endif /* NEARBY_FP_ENABLE_SPOT */

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
