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

#include "core/internal/mediums/advertisement_read_result.h"

#include "platform/api/platform.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

using TestPlatform = platform::ImplementationPlatform;

constexpr char kAdvertisementBytes[] = {0x0A, 0x0B, 0x0C};

// Default values may be too big and impractical to wait for in the test.
// For the test platform, we redefine them to some reasonable values.
const absl::Duration kAdvertisementBaseBackoffDuration =
    absl::Milliseconds(1000);  // 1 second
const absl::Duration kAdvertisementMaxBackoffDuration =
    absl::Milliseconds(6000);  // 6 seconds

template <>
const std::int64_t AdvertisementReadResult<
    TestPlatform>::kAdvertisementMaxBackoffDurationMillis =
        ToInt64Milliseconds(kAdvertisementMaxBackoffDuration);
template <>
const std::int64_t
    AdvertisementReadResult<
        TestPlatform>::kAdvertisementBaseBackoffDurationMillis =
        ToInt64Milliseconds(kAdvertisementBaseBackoffDuration);

TEST(AdvertisementReadResultTest, AdvertisementExists) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  std::int32_t slot = 6;
  advertisement_read_result.addAdvertisement(
      slot,
      MakeConstPtr(new ByteArray(kAdvertisementBytes,
                                 sizeof(kAdvertisementBytes) / sizeof(char))));

  ASSERT_TRUE(advertisement_read_result.hasAdvertisement(slot));
}

TEST(AdvertisementReadResultTest, AdvertisementNonExistent) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  std::int32_t slot = 6;

  ASSERT_FALSE(advertisement_read_result.hasAdvertisement(slot));
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusInitialized) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<TestPlatform>::RetryStatus::RETRY);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusSuccess) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<
                TestPlatform>::RetryStatus::PREVIOUSLY_SUCCEEDED);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusTooSoon) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Sleep for some time, but not long enough to warrant a retry.
  absl::SleepFor(absl::Milliseconds(
      absl::ToInt64Milliseconds(kAdvertisementBaseBackoffDuration) / 2));

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<TestPlatform>::RetryStatus::TOO_SOON);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusRetry) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Sleep long enough to warrant a retry.
  absl::SleepFor(kAdvertisementBaseBackoffDuration);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<TestPlatform>::RetryStatus::RETRY);
}

TEST(AdvertisementReadResultTest, ReportStatusExponentialBackoff) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Record an additional failure so our backoff duration increases.
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Sleep for the backoff duration. We shouldn't trigger a retry because the
  // backoff should have increased from failing a second time.
  absl::SleepFor(kAdvertisementBaseBackoffDuration);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<TestPlatform>::RetryStatus::TOO_SOON);
}

TEST(AdvertisementReadResultTest, ReportStatusExponentialBackoffMax) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Record an absurd amount of failures so we hit the maximum backoff duration.
  for (std::int32_t i = 0; i < 1000; i++) {
    advertisement_read_result.recordLastReadStatus(/* is_success= */ false);
  }

  // Sleep for the maximum backoff duration. This should be enough to warrant a
  // retry.
  absl::SleepFor(kAdvertisementMaxBackoffDuration);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<TestPlatform>::RetryStatus::RETRY);
}

TEST(AdvertisementReadResultTest, GetDurationSinceRead) {
  AdvertisementReadResult<TestPlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  std::int64_t sleepTime = 420;
  absl::SleepFor(absl::Milliseconds(sleepTime));

  ASSERT_GE(advertisement_read_result.getDurationSinceReadMillis(), sleepTime);
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
