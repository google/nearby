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

#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"

#include "gtest/gtest.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr char kAdvertisementBytes[] = "\x0A\x0B\x0C";

// Default values may be too big and impractical to wait for in the test.
// For the test platform, we redefine them to some reasonable values.
const absl::Duration kAdvertisementBaseBackoffDuration = absl::Seconds(1);
const absl::Duration kAdvertisementMaxBackoffDuration = absl::Seconds(6);

const AdvertisementReadResult::Config test_config{
    .backoff_multiplier =
        AdvertisementReadResult::kDefaultConfig.backoff_multiplier,
    .base_backoff_duration = kAdvertisementBaseBackoffDuration,
    .max_backoff_duration = kAdvertisementMaxBackoffDuration,
};

TEST(AdvertisementReadResultTest, AdvertisementExists) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ true);

  int slot = 6;
  advertisement_read_result.AddAdvertisement(slot,
                                             ByteArray(kAdvertisementBytes));

  EXPECT_TRUE(advertisement_read_result.HasAdvertisement(slot));
}

TEST(AdvertisementReadResultTest, AdvertisementNonExistent) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ true);

  int slot = 6;

  EXPECT_FALSE(advertisement_read_result.HasAdvertisement(slot));
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusInitialized) {
  AdvertisementReadResult advertisement_read_result(test_config);

  EXPECT_EQ(advertisement_read_result.EvaluateRetryStatus(),
            AdvertisementReadResult::RetryStatus::kRetry);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusSuccess) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ true);

  EXPECT_EQ(advertisement_read_result.EvaluateRetryStatus(),
            AdvertisementReadResult::RetryStatus::kPreviouslySucceeded);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusTooSoon) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ false);

  // Sleep for some time, but not long enough to warrant a retry.
  absl::SleepFor(kAdvertisementBaseBackoffDuration / 2);

  EXPECT_EQ(advertisement_read_result.EvaluateRetryStatus(),
            AdvertisementReadResult::RetryStatus::kTooSoon);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusRetry) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ false);

  // Sleep long enough to warrant a retry.
  absl::SleepFor(kAdvertisementBaseBackoffDuration);

  EXPECT_EQ(advertisement_read_result.EvaluateRetryStatus(),
            AdvertisementReadResult::RetryStatus::kRetry);
}

TEST(AdvertisementReadResultTest, ReportStatusExponentialBackoff) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ false);

  // Record an additional failure so our backoff duration increases.
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ false);

  // Sleep for the backoff duration. We shouldn't trigger a retry because the
  // backoff should have increased from failing a second time.
  absl::SleepFor(kAdvertisementBaseBackoffDuration);

  EXPECT_EQ(advertisement_read_result.EvaluateRetryStatus(),
            AdvertisementReadResult::RetryStatus::kTooSoon);
}

TEST(AdvertisementReadResultTest, ReportStatusExponentialBackoffMax) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ false);

  // Record an absurd amount of failures so we hit the maximum backoff duration.
  for (int i = 0; i < 1000; i++) {
    advertisement_read_result.RecordLastReadStatus(/* is_success= */ false);
  }

  // Sleep for the maximum backoff duration. This should be enough to warrant a
  // retry.
  absl::SleepFor(kAdvertisementMaxBackoffDuration);

  EXPECT_EQ(advertisement_read_result.EvaluateRetryStatus(),
            AdvertisementReadResult::RetryStatus::kRetry);
}

TEST(AdvertisementReadResultTest, GetDurationSinceRead) {
  AdvertisementReadResult advertisement_read_result(test_config);
  advertisement_read_result.RecordLastReadStatus(/* is_success= */ true);

  absl::Duration sleepTime = absl::Milliseconds(420);
  absl::SleepFor(sleepTime);

  EXPECT_GE(advertisement_read_result.GetDurationSinceRead(), sleepTime);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
