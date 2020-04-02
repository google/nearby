#include "core/internal/mediums/advertisement_read_result.h"

#include "platform/impl/default/default_platform.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace {

class SampleSystemClock : public SystemClock {
 public:
  SampleSystemClock() {}
  ~SampleSystemClock() override {}

  std::int64_t elapsedRealtime() override {
    return absl::ToUnixMillis(absl::Now());
  }
};

class SamplePlatform {
 public:
  static Ptr<Lock> createLock() { return DefaultPlatform::createLock(); }
  static Ptr<SystemClock> createSystemClock() {
    return MakePtr(new SampleSystemClock());
  }
};

// We keep a copy of these constants because this is an old-school test (so we
// can't delare it as a friend class of AdvertisementReadResult).
const absl::Duration kAdvertisementBaseBackoffDuration =
    absl::Milliseconds(1000);  // 1 second
const absl::Duration kAdvertisementMaxBackoffDuration =
    absl::Milliseconds(6000);  // 6 seconds
const char kAdvertisementBytes[] = {0x0A, 0x0B, 0x0C};

template<>
const std::int64_t AdvertisementReadResult<
    SamplePlatform>::kAdvertisementBaseBackoffDurationMillis =
    absl::ToInt64Milliseconds(kAdvertisementBaseBackoffDuration);

template<>
const std::int64_t AdvertisementReadResult<
    SamplePlatform>::kAdvertisementMaxBackoffDurationMillis =
    absl::ToInt64Milliseconds(kAdvertisementMaxBackoffDuration);

TEST(AdvertisementReadResultTest, AdvertisementExists) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  std::int32_t slot = 6;
  advertisement_read_result.addAdvertisement(
      slot,
      MakeConstPtr(new ByteArray(kAdvertisementBytes,
                                 sizeof(kAdvertisementBytes) / sizeof(char))));

  ASSERT_TRUE(advertisement_read_result.hasAdvertisement(slot));
}

TEST(AdvertisementReadResultTest, AdvertisementNonExistent) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  std::int32_t slot = 6;

  ASSERT_FALSE(advertisement_read_result.hasAdvertisement(slot));
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusInitialized) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<SamplePlatform>::RetryStatus::RETRY);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusSuccess) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<
                SamplePlatform>::RetryStatus::PREVIOUSLY_SUCCEEDED);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusTooSoon) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Sleep for some time, but not long enough to warrant a retry.
  absl::SleepFor(absl::Milliseconds(
      absl::ToInt64Milliseconds(kAdvertisementBaseBackoffDuration) / 2));

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<SamplePlatform>::RetryStatus::TOO_SOON);
}

TEST(AdvertisementReadResultTest, EvaluateRetryStatusRetry) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Sleep long enough to warrant a retry.
  absl::SleepFor(kAdvertisementBaseBackoffDuration);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<SamplePlatform>::RetryStatus::RETRY);
}

TEST(AdvertisementReadResultTest, ReportStatusExponentialBackoff) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Record an additional failure so our backoff duration increases.
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Sleep for the backoff duration. We shouldn't trigger a retry because the
  // backoff should have increased from failing a second time.
  absl::SleepFor(kAdvertisementBaseBackoffDuration);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<SamplePlatform>::RetryStatus::TOO_SOON);
}

TEST(AdvertisementReadResultTest, ReportStatusExponentialBackoffMax) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ false);

  // Record an absurd amount of failures so we hit the maximum backoff duration.
  for (std::int32_t i = 0; i < 1000; i++) {
    advertisement_read_result.recordLastReadStatus(/* is_success= */ false);
  }

  // Sleep for the maximum backoff duration. This should be enough to warrant a
  // retry.
  absl::SleepFor(kAdvertisementMaxBackoffDuration);

  ASSERT_EQ(advertisement_read_result.evaluateRetryStatus(),
            AdvertisementReadResult<SamplePlatform>::RetryStatus::RETRY);
}

TEST(AdvertisementReadResultTest, GetDurationSinceRead) {
  AdvertisementReadResult<SamplePlatform> advertisement_read_result;
  advertisement_read_result.recordLastReadStatus(/* is_success= */ true);

  std::int64_t sleepTime = 420;
  absl::SleepFor(absl::Milliseconds(sleepTime));

  ASSERT_GE(advertisement_read_result.getDurationSinceReadMillis(), sleepTime);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
