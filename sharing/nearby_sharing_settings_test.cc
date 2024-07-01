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
#include "absl/types/span.h"
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
    if (key == prefs::kNearbySharingEnabledName) {
      enabled_ = data.value.as_bool;
    } else if (key ==
               prefs::kNearbySharingFastInitiationNotificationStateName) {
      fast_initiation_notification_state_ =
          static_cast<FastInitiationNotificationState>(data.value.as_int64);
    } else if (key == prefs::kNearbySharingDataUsageName) {
      data_usage_ = static_cast<DataUsage>(data.value.as_int64);
    } else if (key == prefs::kNearbySharingCustomSavePath) {
      custom_save_path_ = data.value.as_string;
    } else if (key == prefs::kNearbySharingBackgroundVisibilityName) {
      visibility_ = static_cast<DeviceVisibility>(data.value.as_int64);
    } else if (key == prefs::kNearbySharingOnboardingCompleteName) {
      is_onboarding_complete_ = data.value.as_bool;
    } else if (key == prefs::kNearbySharingDeviceNameName) {
      device_name_ = data.value.as_string;
    }
  }

  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override {
    absl::MutexLock lock(&mutex_);
    is_fast_initiation_notification_hardware_supported_ = is_supported;
  }

  bool enabled() const {
    absl::MutexLock lock(&mutex_);
    return enabled_;
  }

  void set_enabled(bool enabled) {
    absl::MutexLock lock(&mutex_);
    enabled_ = enabled;
  }

  FastInitiationNotificationState fast_initiation_notification_state() const {
    absl::MutexLock lock(&mutex_);
    return fast_initiation_notification_state_;
  }

  bool is_fast_initiation_notification_hardware_supported() const {
    absl::MutexLock lock(&mutex_);
    return is_fast_initiation_notification_hardware_supported_;
  }

  bool is_onboarding_complete() const {
    absl::MutexLock lock(&mutex_);
    return is_onboarding_complete_;
  }

  const std::string& device_name() const {
    absl::MutexLock lock(&mutex_);
    return device_name_;
  }

  const std::string& custom_save_path() const {
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

  bool enabled_ ABSL_GUARDED_BY(mutex_) = false;
  FastInitiationNotificationState fast_initiation_notification_state_
      ABSL_GUARDED_BY(mutex_) =
          FastInitiationNotificationState::ENABLED_FAST_INIT;
  bool is_fast_initiation_notification_hardware_supported_
      ABSL_GUARDED_BY(mutex_) = false;
  bool is_onboarding_complete_ ABSL_GUARDED_BY(mutex_) = false;
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

  void SetIsOnboardingComplete(bool is_complete) {
    preference_manager_.SetBoolean(
        prefs::kNearbySharingOnboardingCompleteName, is_complete);
  }

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

TEST_F(NearbyShareSettingsTest, GetAndSetEnabled) {
  EXPECT_EQ(observer_.enabled(), false);
  settings()->SetIsOnboardingComplete(true, []() {});
  settings()->SetEnabled(true);
  EXPECT_EQ(settings()->GetEnabled(), true);
  Flush();
  EXPECT_EQ(observer_.enabled(), true);

  bool enabled = false;
  settings()->GetEnabled([&enabled](bool result) { enabled = result; });
  EXPECT_EQ(enabled, true);

  settings()->SetEnabled(false);
  EXPECT_EQ(settings()->GetEnabled(), false);
  Flush();
  EXPECT_EQ(observer_.enabled(), false);

  settings()->GetEnabled([&enabled](bool result) { enabled = result; });
  EXPECT_EQ(enabled, false);

  // Verify that setting the value to false again value doesn't trigger an
  // observer event.
  observer_.set_enabled(true);
  settings()->SetEnabled(false);
  EXPECT_EQ(settings()->GetEnabled(), false);
  Flush();
  // the observers' value should not have been updated.
  EXPECT_EQ(observer_.enabled(), true);
}

TEST_F(NearbyShareSettingsTest, GetAndSetFastInitiationNotificationState) {
  // Fast init notifications are enabled by default.
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::ENABLED_FAST_INIT);
  settings()->SetFastInitiationNotificationState(
      FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);
  EXPECT_EQ(FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT,
            settings()->GetFastInitiationNotificationState());
  Flush();
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);

  FastInitiationNotificationState state =
      FastInitiationNotificationState::ENABLED_FAST_INIT;
  settings()->GetFastInitiationNotificationState(
      [&state](FastInitiationNotificationState result) { state = result; });
  EXPECT_EQ(state, FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);
}

TEST_F(NearbyShareSettingsTest,
       ParentFeatureChangesFastInitiationNotificationState) {
  // Fast init notifications are enabled by default.
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::ENABLED_FAST_INIT);
  settings()->SetIsOnboardingComplete(true, []() {});
  settings()->SetEnabled(true);
  Flush();

  // Simulate toggling the parent feature off.
  settings()->SetEnabled(false);
  Flush();
  EXPECT_FALSE(settings()->GetEnabled());
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::DISABLED_BY_FEATURE_FAST_INIT);

  // Simulate toggling the parent feature on.
  settings()->SetEnabled(true);
  Flush();
  EXPECT_TRUE(settings()->GetEnabled());
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::ENABLED_FAST_INIT);
}

TEST_F(NearbyShareSettingsTest,
       ParentFeatureChangesFastInitiationNotificationDisabledByUser) {
  // Fast init notifications are enabled by default.
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::ENABLED_FAST_INIT);

  // Set explicitly disabled by user.
  settings()->SetFastInitiationNotificationState(
      FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);
  Flush();
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);

  // Simulate toggling parent feature on.
  settings()->SetIsOnboardingComplete(true, []() {});
  settings()->SetEnabled(true);
  Flush();

  // The disabled by user flag should persist if the parent feature is enabled.
  EXPECT_EQ(observer_.fast_initiation_notification_state(),
            FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);
}

TEST_F(NearbyShareSettingsTest, GetAndSetCustomSavePath) {
  absl::Notification notification;
  settings()->SetCustomSavePathAsync(
      GetCompatibleU8String(std::filesystem::temp_directory_path().u8string()),
      [&]() { notification.Notify(); });
  Flush();
  EXPECT_TRUE(notification.HasBeenNotified());
  settings()->GetCustomSavePathAsync([&](absl::string_view path) {
    observer_.OnSettingChanged(
        prefs::kNearbySharingCustomSavePath,
        NearbyShareSettings::Observer::Data(std::string(path)));
  });
  Flush();
  EXPECT_EQ(
      observer_.custom_save_path(),
      GetCompatibleU8String(std::filesystem::temp_directory_path().u8string()));
}

TEST_F(NearbyShareSettingsTest, GetAndSetIsOnboardingComplete) {
  EXPECT_FALSE(observer_.is_onboarding_complete());
  SetIsOnboardingComplete(true);
  EXPECT_TRUE(settings()->IsOnboardingComplete());
  Flush();
  EXPECT_TRUE(observer_.is_onboarding_complete());

  bool is_complete = false;
  settings()->IsOnboardingComplete(
      [&is_complete](bool result) { is_complete = result; });
  EXPECT_TRUE(is_complete);
}

TEST_F(NearbyShareSettingsTest, GetAndSetIsFastInitiationHardwareSupported) {
  EXPECT_FALSE(observer_.is_fast_initiation_notification_hardware_supported());
  settings()->SetIsFastInitiationHardwareSupported(true);

  Flush();
  EXPECT_TRUE(observer_.is_fast_initiation_notification_hardware_supported());

  bool is_supported = false;
  settings()->GetIsFastInitiationHardwareSupported(
      [&is_supported](bool result) { is_supported = result; });
  EXPECT_TRUE(is_supported);
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
  settings()->GetDeviceName(
      [&name](absl::string_view result) { name = std::string(result); });
  EXPECT_EQ(kDefaultDeviceName, name);

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

  settings()->GetDeviceName(
      [&name](absl::string_view result) { name = std::string(result); });
  EXPECT_EQ(name, "d");
}

TEST_F(NearbyShareSettingsTest, GetAndSetDataUsage) {
  EXPECT_EQ(observer_.data_usage(), DataUsage::UNKNOWN_DATA_USAGE);
  settings()->SetDataUsage(DataUsage::OFFLINE_DATA_USAGE);
  EXPECT_EQ(settings()->GetDataUsage(), DataUsage::OFFLINE_DATA_USAGE);
  Flush();
  EXPECT_EQ(observer_.data_usage(), DataUsage::OFFLINE_DATA_USAGE);

  DataUsage data_usage = DataUsage::UNKNOWN_DATA_USAGE;
  settings()->GetDataUsage(
      [&data_usage](DataUsage usage) { data_usage = usage; });
  EXPECT_EQ(data_usage, DataUsage::OFFLINE_DATA_USAGE);
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

  DeviceVisibility visibility = DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED;
  settings()->GetVisibility(
      [&visibility](DeviceVisibility result) { visibility = result; });
  EXPECT_EQ(visibility, DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
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
  EXPECT_EQ(settings()->GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
}

TEST_F(NearbyShareSettingsTest,
       GetFallbackVisibilityReturnsUnspecifiedIfPermanent) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);

  // Since this is permanent, we shouldn't have a fallback.
  EXPECT_EQ(settings()->GetFallbackVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
}

TEST_F(NearbyShareSettingsTest,
       TransitionToPermanentNonEveryoneClearsFallback) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Set everyone mode temporarily.
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Verify that the fallback visibility is valid and set to self share.
  EXPECT_EQ(settings()->GetFallbackVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Transition to permanent self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Verify that the fallback visibility is unset.
  EXPECT_EQ(settings()->GetFallbackVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
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
  EXPECT_EQ(settings()->GetFallbackVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED);
}

TEST_F(NearbyShareSettingsTest, TemporaryVisibilityIsCorrect) {
  // Set our initial visibility to self share.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
  // Verify that the visibility is not temporary.
  EXPECT_FALSE(settings()->GetIsTemporarilyVisible());
  // Transition to temporary everyone mode.
  settings()->SetVisibility(
      DeviceVisibility::DEVICE_VISIBILITY_EVERYONE,
      absl::Seconds(prefs::kDefaultMaxVisibilityExpirationSeconds));
  // Verify that the visibility is temporary.
  EXPECT_TRUE(settings()->GetIsTemporarilyVisible());
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
