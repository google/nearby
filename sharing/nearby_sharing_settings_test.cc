// Copyright 2022-2023 Google LLC
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

#include "sharing/nearby_sharing_settings.h"

#include <algorithm>
#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::FastInitiationNotificationState;

constexpr char kDefaultDeviceName[] = "Josh's Chromebook";

class FakeNearbyShareSettingsObserver : public NearbyShareSettings::Observer {
 public:
  void OnSettingChanged(absl::string_view key, const Data& data) override {
    absl::MutexLock lock(&mutex_);
    if (key == prefs::kNearbySharingFastInitiationNotificationStateName) {
      fast_initiation_notification_state_ =
          static_cast<FastInitiationNotificationState>(data.value.as_int64);
    } else if (key == prefs::kNearbySharingDataUsageName) {
      data_usage_ = static_cast<DataUsage>(data.value.as_int64);
    } else if (key == prefs::kNearbySharingCustomSavePath) {
      custom_save_path_ = data.value.as_string;
    } else if (key == prefs::kNearbySharingBackgroundVisibilityName) {
      visibility_ = static_cast<DeviceVisibility>(data.value.as_int64);
    } else if (key == prefs::kNearbySharingDeviceNameName) {
      device_name_ = data.value.as_string;
    }
  }

  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override {
    absl::MutexLock lock(&mutex_);
    is_fast_initiation_notification_hardware_supported_ = is_supported;
  }

  FastInitiationNotificationState fast_initiation_notification_state() const {
    absl::MutexLock lock(&mutex_);
    return fast_initiation_notification_state_;
  }

  bool is_fast_initiation_notification_hardware_supported() const {
    absl::MutexLock lock(&mutex_);
    return is_fast_initiation_notification_hardware_supported_;
  }

  std::string device_name() const {
    absl::MutexLock lock(&mutex_);
    return device_name_;
  }

  std::string custom_save_path() const {
    absl::MutexLock lock(&mutex_);
    return custom_save_path_;
  }

  DataUsage data_usage() const {
    absl::MutexLock lock(&mutex_);
    return data_usage_;
  }

  DeviceVisibility visibility() const {
    absl::MutexLock lock(&mutex_);
    return visibility_;
  }

 private:
  mutable absl::Mutex mutex_;

  FastInitiationNotificationState fast_initiation_notification_state_
      ABSL_GUARDED_BY(mutex_) =
          FastInitiationNotificationState::ENABLED_FAST_INIT;
  bool is_fast_initiation_notification_hardware_supported_
      ABSL_GUARDED_BY(mutex_) = false;
  std::string device_name_ ABSL_GUARDED_BY(mutex_) = "uncalled";
  std::string custom_save_path_ ABSL_GUARDED_BY(mutex_);
  DataUsage data_usage_ ABSL_GUARDED_BY(mutex_) = DataUsage::UNKNOWN_DATA_USAGE;
  DeviceVisibility visibility_ ABSL_GUARDED_BY(mutex_) =
      DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED;
};

class NearbyShareSettingsTest : public ::testing::Test {
 public:
  NearbyShareSettingsTest()
      : local_device_data_manager_(kDefaultDeviceName) {
    FakeTaskRunner::ResetPendingTasksCount();
    prefs::RegisterNearbySharingPrefs(preference_manager_);
    nearby_share_settings_ = std::make_unique<NearbyShareSettings>(
        &context_, context_.GetClock(), fake_device_info_, preference_manager_,
        &local_device_data_manager_);

    nearby_share_settings_->AddSettingsObserver(&observer_);
  }

  ~NearbyShareSettingsTest() override = default;

  void TearDown() override { Flush(); }

  NearbyShareSettings* settings() { return nearby_share_settings_.get(); }

  void SetVisibilityExpirationPreference(int expiration) {
    preference_manager_.SetInteger(
        prefs::kNearbySharingBackgroundVisibilityExpirationSeconds, expiration);
  }

  void SetCustomSavePath(absl::string_view path) {
    preference_manager_.SetString(
        prefs::kNearbySharingCustomSavePath, path);
  }

  // Waits for running tasks to complete.
  void Flush() {
    absl::SleepFor(absl::Seconds(1));
    FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Milliseconds(200));
  }

  void FastForward(absl::Duration duration) {
    context_.fake_clock()->FastForward(duration);
  }

  bool Contains(std::vector<std::string> v, std::string val) {
    if (std::find(v.begin(), v.end(), val) != v.end()) {
      return true;
    }
    return false;
  }

 protected:
  nearby::FakeDeviceInfo fake_device_info_;
  nearby::FakePreferenceManager preference_manager_;
  FakeContext context_;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_;
  FakeNearbyShareSettingsObserver observer_;
  std::unique_ptr<NearbyShareSettings> nearby_share_settings_;
};

TEST_F(NearbyShareSettingsTest, GetAndSetCustomSavePath) {
  absl::Notification notification;
  std::string save_path =
      GetCompatibleU8String(std::filesystem::temp_directory_path().u8string());
  settings()->SetCustomSavePathAsync(save_path,
                                     [&]() { notification.Notify(); });
  Flush();
  EXPECT_TRUE(notification.HasBeenNotified());

  EXPECT_EQ(settings()->GetCustomSavePath(), save_path);
}

TEST_F(NearbyShareSettingsTest, GetAndSetIsFastInitiationHardwareSupported) {
  EXPECT_FALSE(observer_.is_fast_initiation_notification_hardware_supported());
  settings()->SetIsFastInitiationHardwareSupported(true);

  Flush();
  EXPECT_TRUE(observer_.is_fast_initiation_notification_hardware_supported());

  EXPECT_TRUE(settings()->is_fast_initiation_hardware_supported());
}

TEST_F(NearbyShareSettingsTest, ValidateDeviceName) {
  auto result = DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      DeviceNameValidationResult::kErrorEmpty);
  settings()->ValidateDeviceName(
      "", [&result](DeviceNameValidationResult res) { result = res; });
  EXPECT_EQ(result, DeviceNameValidationResult::kErrorEmpty);

  local_device_data_manager_.set_next_validation_result(
      DeviceNameValidationResult::kValid);
  settings()->ValidateDeviceName(
      "this string is 32 bytes in UTF-8",
      [&result](DeviceNameValidationResult res) { result = res; });
  EXPECT_EQ(result, DeviceNameValidationResult::kValid);
}

TEST_F(NearbyShareSettingsTest, GetAndSetDeviceName) {
  std::string name = "not_the_default";
  EXPECT_EQ(kDefaultDeviceName, settings()->GetDeviceName());

  // When we get a validation error, setting the name should not succeed.
  EXPECT_EQ(observer_.device_name(), "uncalled");
  auto result = DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      DeviceNameValidationResult::kErrorEmpty);
  settings()->SetDeviceName(
      "", [&result](DeviceNameValidationResult res) { result = res; });
  EXPECT_EQ(result, DeviceNameValidationResult::kErrorEmpty);
  EXPECT_EQ(settings()->GetDeviceName(), kDefaultDeviceName);

  // When the name is valid, the setting should succeed.
  EXPECT_EQ(observer_.device_name(), "uncalled");
  result = DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      DeviceNameValidationResult::kValid);
  settings()->SetDeviceName(
      "d", [&result](DeviceNameValidationResult res) { result = res; });
  EXPECT_EQ(result, DeviceNameValidationResult::kValid);
  EXPECT_EQ(settings()->GetDeviceName(), "d");

  Flush();
  EXPECT_EQ(observer_.device_name(), "d");

  EXPECT_EQ(settings()->GetDeviceName(), "d");
}

TEST_F(NearbyShareSettingsTest, GetAndSetDataUsage) {
  EXPECT_EQ(observer_.data_usage(), DataUsage::UNKNOWN_DATA_USAGE);
  settings()->SetDataUsage(DataUsage::OFFLINE_DATA_USAGE);
  EXPECT_EQ(settings()->GetDataUsage(), DataUsage::OFFLINE_DATA_USAGE);
  Flush();
  EXPECT_EQ(observer_.data_usage(), DataUsage::OFFLINE_DATA_USAGE);

  EXPECT_EQ(settings()->GetDataUsage(), DataUsage::OFFLINE_DATA_USAGE);
}

TEST_F(NearbyShareSettingsTest, GetAndSetVisibility) {
  EXPECT_EQ(observer_.visibility(),
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  EXPECT_EQ(settings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  Flush();
  EXPECT_EQ(observer_.visibility(),
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);

  EXPECT_EQ(settings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
}

TEST_F(NearbyShareSettingsTest,
       SetTemporaryVisibilityExpiresAndRestoresOriginal) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Set everyone mode temporarily.
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Fast forward to the expiration time.
  FastForward(absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds + 1));
  // Verify that the visibility has expired and we are back to self share.
  Flush();
  EXPECT_EQ(settings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  EXPECT_EQ(settings()->GetFallbackVisibility().visibility,
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
}

TEST_F(NearbyShareSettingsTest,
       SetPersistentThenTemporaryVisibilityExpiresAndRestoresOriginal) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Set persistent everyone mode.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  // Set everyone mode temporarily.
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Fast forward to the expiration time.
  FastForward(absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds + 1));
  // Verify that the visibility has expired and we are back to self share.
  Flush();
  EXPECT_EQ(settings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  EXPECT_EQ(settings()->GetFallbackVisibility().visibility,
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
}

TEST_F(NearbyShareSettingsTest,
       GetFallbackVisibilityReturnsUnspecifiedIfPermanent) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);

  // Since this is permanent, we shouldn't have a fallback.
  NearbyShareSettings::FallbackVisibilityInfo fallback_visibility =
      settings()->GetFallbackVisibility();
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
  EXPECT_EQ(fallback_visibility.fallback_time, absl::UnixEpoch());
}

TEST_F(NearbyShareSettingsTest,
       TransitionToPermanentNonEveryoneClearsFallback) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Set everyone mode temporarily.
  absl::Time now = context_.GetClock()->Now();
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Verify that the fallback visibility is valid and set to self share.
  NearbyShareSettings::FallbackVisibilityInfo fallback_visibility =
      settings()->GetFallbackVisibility();
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  absl::Time expected_fallback_time =
      now + absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds);
  EXPECT_LT(fallback_visibility.fallback_time - expected_fallback_time,
            absl::Seconds(1));
  // Transition to permanent self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Verify that the fallback visibility is unset.
  fallback_visibility = settings()->GetFallbackVisibility();
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
  EXPECT_EQ(fallback_visibility.fallback_time, absl::UnixEpoch());
}

TEST_F(NearbyShareSettingsTest,
       TransitionToPermanentEveryoneDoesNotClearFallback) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Transition to temporary everyone mode.
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Transition to permanent everyone mode.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  // Verify that the saved fallback visibility is intact, since we can go back
  // to temporary.
  EXPECT_EQ(preference_manager_.GetInteger(
                prefs::kNearbySharingBackgroundFallbackVisibilityName,
                prefs::kDefaultFallbackVisibility),
            static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE));
  // Verify that the fallback visibility is unspecified.
  NearbyShareSettings::FallbackVisibilityInfo fallback_visibility =
      settings()->GetFallbackVisibility();
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
  EXPECT_EQ(fallback_visibility.fallback_time, absl::UnixEpoch());
}

TEST_F(NearbyShareSettingsTest, TemporaryVisibilityIsCorrect) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Verify that the visibility is not temporary via absence of fallback
  // visibility.
  NearbyShareSettings::FallbackVisibilityInfo fallback_visibility =
      settings()->GetFallbackVisibility();
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
  EXPECT_EQ(fallback_visibility.fallback_time, absl::UnixEpoch());
  // Transition to temporary everyone mode.
  absl::Time now = context_.GetClock()->Now();
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Verify that the visibility is temporary via fallback visibility being
  // present.
  fallback_visibility = settings()->GetFallbackVisibility();
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  absl::Time expected_fallback_time =
      now + absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds);
  EXPECT_LT(fallback_visibility.fallback_time - expected_fallback_time,
            absl::Seconds(1));
}

TEST(NearbyShareVisibilityTest, RestoresFallbackVisibility_ExpiredTimer) {
  // Create Nearby Share settings dependencies.
  FakeContext context;
  FakeDeviceInfo fake_device_info;
  FakePreferenceManager preference_manager;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager(
      kDefaultDeviceName);
  // Set everyone mode temporarily.
  preference_manager.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE));
  // Set expiration to 10 seconds ago.
  preference_manager.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityExpirationSeconds,
      absl::ToUnixSeconds(context.GetClock()->Now() - absl::Seconds(10)));
  // Set fallback visibility to self share.
  preference_manager.SetInteger(
      prefs::kNearbySharingBackgroundFallbackVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE));
  // Create a Nearby Share settings instance.
  NearbyShareSettings settings(&context, context.GetClock(), fake_device_info,
                               preference_manager, &local_device_data_manager);

  // Make sure we restore the correct visibility.
  EXPECT_EQ(settings.GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
}

TEST(NearbyShareVisibilityTest, RestoresFallbackVisibility_FutureTimer) {
  // Create Nearby Share settings dependencies.
  FakeContext context;
  FakeDeviceInfo fake_device_info;
  FakePreferenceManager preference_manager;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager(
      kDefaultDeviceName);
  // Set everyone mode temporarily.
  preference_manager.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE));
  // Set expiration to 10 seconds in the future.
  preference_manager.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityExpirationSeconds,
      absl::ToUnixSeconds(context.GetClock()->Now() + absl::Seconds(10)));
  // Set fallback visibility to self share.
  preference_manager.SetInteger(
      prefs::kNearbySharingBackgroundFallbackVisibilityName,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE));
  // Create a Nearby Share settings instance.
  NearbyShareSettings settings(&context, context.GetClock(), fake_device_info,
                               preference_manager, &local_device_data_manager);

  // Make sure we restore the correct visibility.
  EXPECT_EQ(settings.GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
