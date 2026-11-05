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
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/base/files.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_device_settings.h"
#include "sharing/analytics/analytics_information.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/attachment_container.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/public/pref_names.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/share_target.h"

namespace nearby {
namespace sharing {
namespace {
using ::location::nearby::proto::sharing::ShowNotificationStatus;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::FastInitiationNotificationState;
using ::protobuf_matchers::EqualsProto;
using ::testing::NiceMock;

constexpr char kDefaultDeviceName[] = "Josh's Chromebook";

class MockAnalyticsRecorder : public analytics::AnalyticsRecorder {
 public:
  void NewEstablishConnection(
      int64_t, location::nearby::proto::sharing::EstablishConnectionStatus,
      const ShareTarget&, int, int, int64_t) override {}
  void NewAcceptAgreements() override {}
  void NewDeclineAgreements() override {}
  void NewAddContact() override {}
  void NewRemoveContact() override {}
  void NewTapFeedback() override {}
  void NewTapHelp() override {}
  void NewLaunchDeviceContactConsent(
      location::nearby::proto::sharing::ConsentAcceptanceStatus) override {}
  void NewAdvertiseDevicePresenceEnd(int64_t) override {}
  void NewAdvertiseDevicePresenceStart(
      int64_t, nearby::sharing::proto::DeviceVisibility,
      location::nearby::proto::sharing::SessionStatus,
      nearby::sharing::proto::DataUsage) override {}
  void NewDescribeAttachments(const AttachmentContainer&) override {}
  void NewDiscoverShareTarget(const ShareTarget&, int64_t, int64_t,
                              int64_t) override {}
  void NewEnableNearbySharing(
      location::nearby::proto::sharing::NearbySharingStatus) override {}
  void NewOpenReceivedAttachments(const AttachmentContainer&,
                                  int64_t) override {}
  void NewProcessReceivedAttachmentsEnd(
      int64_t,
      location::nearby::proto::sharing::ProcessReceivedAttachmentsStatus)
      override {}
  void NewReceiveAttachmentsEnd(
      int64_t, int64_t,
      location::nearby::proto::sharing::AttachmentTransmissionStatus) override {
  }
  void NewReceiveAttachmentsStart(int64_t,
                                  const AttachmentContainer&) override {}
  void NewReceiveFastInitialization(int64_t) override {}
  void NewAcceptFastInitialization() override {}
  void NewDismissFastInitialization() override {}
  void NewReceiveIntroduction(
      int64_t, const ShareTarget&, location::nearby::proto::sharing::OSType,
      location::nearby::proto::sharing::SharingUseCase,
      location::nearby::proto::sharing::PowerStatus) override {}
  void NewRespondToIntroduction(
      location::nearby::proto::sharing::ResponseToIntroduction,
      int64_t) override {}
  void NewTapPrivacyNotification() override {}
  void NewDismissPrivacyNotification() override {}
  void NewScanForShareTargetsEnd(int64_t) override {}
  void NewScanForShareTargetsStart(
      int64_t, location::nearby::proto::sharing::SessionStatus,
      analytics::AnalyticsInformation) override {}
  void NewSendAttachmentsEnd(
      int64_t, int64_t, const ShareTarget&,
      location::nearby::proto::sharing::AttachmentTransmissionStatus, int, int,
      int64_t, location::nearby::proto::sharing::ConnectionLayerStatus,
      location::nearby::proto::sharing::OSType) override {}
  void NewSendAttachmentsStart(int64_t, const AttachmentContainer&, int, int,
                               bool) override {}
  void NewSendFastInitialization() override {}
  void NewSendStart(int64_t, int, int, const ShareTarget&) override {}
  void NewSendIntroduction(ShareTargetType, int64_t,
                           location::nearby::proto::sharing::DeviceRelationship,
                           location::nearby::proto::sharing::OSType) override {}
  void NewSendIntroduction(
      int64_t, const ShareTarget&, int, int,
      location::nearby::proto::sharing::OSType,
      location::nearby::proto::sharing::SharingUseCase,
      location::nearby::proto::sharing::PowerStatus) override {}
  void NewSetVisibility(nearby::sharing::proto::DeviceVisibility,
                        nearby::sharing::proto::DeviceVisibility,
                        int64_t) override {}
  void NewDeviceSettings(analytics::AnalyticsDeviceSettings) override {}
  MOCK_METHOD(void, NewSetDataUsage,
              (nearby::sharing::proto::DataUsage original_preference,
               nearby::sharing::proto::DataUsage preference),
              (override));
  void NewAddQuickSettingsTile() override {}
  void NewRemoveQuickSettingsTile() override {}
  void NewTapQuickSettingsTile() override {}
  MOCK_METHOD(
      void, NewToggleShowNotification,
      (location::nearby::proto::sharing::ShowNotificationStatus prev_status,
       location::nearby::proto::sharing::ShowNotificationStatus current_status),
      (override));
  MOCK_METHOD(void, NewSetDeviceName, (int device_name_size), (override));
  void NewRequestSettingPermissions(
      location::nearby::proto::sharing::PermissionRequestType,
      location::nearby::proto::sharing::PermissionRequestResult) override {}
  void NewInstallAPKStatus(
      location::nearby::proto::sharing::InstallAPKStatus,
      location::nearby::proto::sharing::ApkSource) override {}
  void NewVerifyAPKStatus(
      location::nearby::proto::sharing::VerifyAPKStatus,
      location::nearby::proto::sharing::ApkSource) override {}
  void NewRpcCallStatus(absl::string_view, RpcDirection, int,
                        absl::Duration) override {}
  void NewPairDevicesStatus(
      location::nearby::proto::sharing::PairingResultStatus,
      absl::Duration) override {}
  void NewUnpairDevice(
      location::nearby::proto::sharing::UnpairTriggerType) override {}
  int64_t GenerateNextId() override { return 0; }
};

class FakeNearbyShareSettingsObserver : public NearbyShareSettings::Observer {
 public:
  void OnSettingChanged(absl::string_view key, const Data& data) override {
    absl::MutexLock lock(mutex_);
    if (key == PrefNames::kFastInitiationNotificationState) {
      fast_initiation_notification_state_ =
          static_cast<FastInitiationNotificationState>(data.value.as_int64);
    } else if (key == PrefNames::kDataUsage) {
      data_usage_ = static_cast<DataUsage>(data.value.as_int64);
    } else if (key == PrefNames::kCustomSavePath) {
      custom_save_path_ = data.value.as_string;
    } else if (key == PrefNames::kVisibility) {
      visibility_ = static_cast<DeviceVisibility>(data.value.as_int64);
    } else if (key == PrefNames::kDeviceName) {
      device_name_ = data.value.as_string;
    }
  }

  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override {
    absl::MutexLock lock(mutex_);
    is_fast_initiation_notification_hardware_supported_ = is_supported;
  }

  FastInitiationNotificationState fast_initiation_notification_state() const {
    absl::MutexLock lock(mutex_);
    return fast_initiation_notification_state_;
  }

  bool is_fast_initiation_notification_hardware_supported() const {
    absl::MutexLock lock(mutex_);
    return is_fast_initiation_notification_hardware_supported_;
  }

  std::string device_name() const {
    absl::MutexLock lock(mutex_);
    return device_name_;
  }

  std::string custom_save_path() const {
    absl::MutexLock lock(mutex_);
    return custom_save_path_;
  }

  DataUsage data_usage() const {
    absl::MutexLock lock(mutex_);
    return data_usage_;
  }

  DeviceVisibility visibility() const {
    absl::MutexLock lock(mutex_);
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
      : local_device_data_manager_(kDefaultDeviceName),
        fake_task_runner_(&fake_clock_, /*count=*/1) {
    prefs::RegisterNearbySharingPrefs(preference_manager_);
    nearby_share_settings_ = std::make_unique<NearbyShareSettings>(
        &fake_task_runner_, &fake_clock_, fake_device_info_,
        preference_manager_, &local_device_data_manager_, &analytics_recorder_);

    nearby_share_settings_->AddSettingsObserver(&observer_);
  }

  ~NearbyShareSettingsTest() override = default;

  void TearDown() override { Flush(); }

  NearbyShareSettings* settings() { return nearby_share_settings_.get(); }

  void SetVisibilityExpirationPreference(int expiration) {
    preference_manager_.SetInteger(PrefNames::kVisibilityExpirationSeconds,
                                   expiration);
  }

  void SetCustomSavePath(absl::string_view path) {
    preference_manager_.SetString(PrefNames::kCustomSavePath, path);
  }

  // Waits for running tasks to complete.
  void Flush() {
    absl::SleepFor(absl::Seconds(1));
    fake_task_runner_.SyncWithTimeout(absl::Milliseconds(200));
  }

  void FastForward(absl::Duration duration) {
    fake_clock_.FastForward(duration);
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
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_;
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_;
  FakeNearbyShareSettingsObserver observer_;
  NiceMock<MockAnalyticsRecorder> analytics_recorder_;
  std::unique_ptr<NearbyShareSettings> nearby_share_settings_;
};

TEST_F(NearbyShareSettingsTest, GetAndSetCustomSavePath) {
  absl::Notification notification;
  std::string save_path = Files::GetTemporaryDirectory().ToString();
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
  absl::Time now = fake_clock_.Now();
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
  EXPECT_EQ(preference_manager_.GetInteger(PrefNames::kFallbackVisibility,
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
  absl::Time now = fake_clock_.Now();
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

TEST_F(NearbyShareSettingsTest, SetVisibilityWithExpirationTooLong) {
  // Set visibility with expiration longer than the max.
  settings()->SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE,
                            absl::Hours(1));
  // Expiration capped at 10minutes.
  absl::Time expected_fallback_time =
      fake_clock_.Now() + absl::Minutes(10);
  NearbyShareSettings::FallbackVisibilityInfo fallback_visibility =
      settings()->GetFallbackVisibility();
  // default visibility was hidden.
  EXPECT_EQ(fallback_visibility.visibility,
            DeviceVisibility::DEVICE_VISIBILITY_HIDDEN);
  absl::Duration time_diff = fallback_visibility.fallback_time -
                             expected_fallback_time;
  EXPECT_LT(absl::AbsDuration(time_diff), absl::Seconds(1));
}

TEST_F(NearbyShareSettingsTest, GetSyncBindingPrefs_NoBindings) {
  EXPECT_THAT(settings()->GetSyncBindingPrefs(),
            EqualsProto(sync::SyncBindingPrefs::default_instance()));
}

TEST_F(NearbyShareSettingsTest, GetSyncBindingPerfs_Success) {
  sync::SyncBindingPrefs sync_binding_prefs;
  sync_binding_prefs.add_sync_bindings()->set_binding_id("binding_id");
  sync_binding_prefs.add_sync_bindings()->set_source_name("source_name");
  sync_binding_prefs.add_sync_bindings()->set_destination_directory(
      "destination_name");
  preference_manager_.SetSyncBindingValue(sync_binding_prefs);
  EXPECT_THAT(settings()->GetSyncBindingPrefs(),
              EqualsProto(sync_binding_prefs));
}

TEST_F(NearbyShareSettingsTest, SetSyncBindingPerfs_Success) {
  sync::SyncBindingPrefs sync_binding_prefs;
  sync_binding_prefs.add_sync_bindings()->set_binding_id("binding_id");
  sync_binding_prefs.add_sync_bindings()->set_source_name("source_name");
  sync_binding_prefs.add_sync_bindings()->set_destination_directory(
      "destination_name");
  settings()->SetSyncBindingPrefs(sync_binding_prefs);
  auto sync_binding_value = preference_manager_.GetSyncBindingValue();
  ASSERT_TRUE(sync_binding_value.has_value());
  EXPECT_THAT(sync_binding_value.value(),
              EqualsProto(sync_binding_prefs));
}

TEST(NearbyShareVisibilityTest, RestoresFallbackVisibility_ExpiredTimer) {
  // Create Nearby Share settings dependencies.
  FakeClock fake_clock;
  FakeTaskRunner fake_task_runner(&fake_clock, /*count=*/1);
  FakeDeviceInfo fake_device_info;
  FakePreferenceManager preference_manager;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager(
      kDefaultDeviceName);
  // Set everyone mode temporarily.
  preference_manager.SetInteger(
      PrefNames::kVisibility,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE));
  // Set expiration to 10 seconds ago.
  preference_manager.SetInteger(
      PrefNames::kVisibilityExpirationSeconds,
      absl::ToUnixSeconds(fake_clock.Now() - absl::Seconds(10)));
  // Set fallback visibility to self share.
  preference_manager.SetInteger(
      PrefNames::kFallbackVisibility,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE));
  // Create a Nearby Share settings instance.
  NearbyShareSettings settings(&fake_task_runner, &fake_clock, fake_device_info,
                               preference_manager, &local_device_data_manager);

  // Make sure we restore the correct visibility.
  EXPECT_EQ(settings.GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
}

TEST(NearbyShareVisibilityTest, RestoresFallbackVisibility_FutureTimer) {
  // Create Nearby Share settings dependencies.
  FakeClock fake_clock;
  FakeTaskRunner fake_task_runner(&fake_clock, /*count=*/1);
  FakeDeviceInfo fake_device_info;
  FakePreferenceManager preference_manager;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager(
      kDefaultDeviceName);
  // Set everyone mode temporarily.
  preference_manager.SetInteger(
      PrefNames::kVisibility,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE));
  // Set expiration to 10 seconds in the future.
  preference_manager.SetInteger(
      PrefNames::kVisibilityExpirationSeconds,
      absl::ToUnixSeconds(fake_clock.Now() + absl::Seconds(10)));
  // Set fallback visibility to self share.
  preference_manager.SetInteger(
      PrefNames::kFallbackVisibility,
      static_cast<int>(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE));
  // Create a Nearby Share settings instance.
  NearbyShareSettings settings(&fake_task_runner, &fake_clock, fake_device_info,
                               preference_manager, &local_device_data_manager);

  // Make sure we restore the correct visibility.
  EXPECT_EQ(settings.GetVisibility(),
            DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
}

TEST_F(NearbyShareSettingsTest,
       SetFastInitiationNotificationStateLogsAnalyticsOnlyWhenStateChanges) {
  ASSERT_EQ(settings()->GetFastInitiationNotificationState(),
            FastInitiationNotificationState::ENABLED_FAST_INIT);

  // Setting the identical state should not emit an analytics event.
  EXPECT_CALL(analytics_recorder_, NewToggleShowNotification).Times(0);
  settings()->SetFastInitiationNotificationState(
      FastInitiationNotificationState::ENABLED_FAST_INIT);
  testing::Mock::VerifyAndClearExpectations(&analytics_recorder_);

  // Transitioning to a new state should emit exactly one analytics event.
  EXPECT_CALL(analytics_recorder_,
              NewToggleShowNotification(ShowNotificationStatus::SHOW,
                                        ShowNotificationStatus::NOT_SHOW))
      .Times(1);
  settings()->SetFastInitiationNotificationState(
      FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);
  testing::Mock::VerifyAndClearExpectations(&analytics_recorder_);

  // Repeating the new state again should not emit another analytics event.
  EXPECT_CALL(analytics_recorder_, NewToggleShowNotification).Times(0);
  settings()->SetFastInitiationNotificationState(
      FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT);
}

TEST_F(NearbyShareSettingsTest,
       SetDeviceNameLogsAnalyticsOnlyWhenValidAndChanged) {
  ASSERT_EQ(settings()->GetDeviceName(), kDefaultDeviceName);

  // Validation failure must not log a device-name change analytics event.
  EXPECT_CALL(analytics_recorder_, NewSetDeviceName).Times(0);
  DeviceNameValidationResult validation_result =
      DeviceNameValidationResult::kValid;
  local_device_data_manager_.set_next_validation_result(
      DeviceNameValidationResult::kErrorEmpty);
  settings()->SetDeviceName(
      "", [&validation_result](DeviceNameValidationResult result) {
        validation_result = result;
      });
  EXPECT_EQ(validation_result, DeviceNameValidationResult::kErrorEmpty);
  testing::Mock::VerifyAndClearExpectations(&analytics_recorder_);

  // Setting the same name as the current device name must not log analytics.
  EXPECT_CALL(analytics_recorder_, NewSetDeviceName).Times(0);
  local_device_data_manager_.set_next_validation_result(
      DeviceNameValidationResult::kValid);
  settings()->SetDeviceName(
      kDefaultDeviceName,
      [&validation_result](DeviceNameValidationResult result) {
        validation_result = result;
      });
  EXPECT_EQ(validation_result, DeviceNameValidationResult::kValid);
  testing::Mock::VerifyAndClearExpectations(&analytics_recorder_);

  // Setting a valid new name must log NewSetDeviceName once with its length.
  constexpr absl::string_view kNewName = "Pixel Tablet";
  EXPECT_CALL(analytics_recorder_, NewSetDeviceName(kNewName.size())).Times(1);
  settings()->SetDeviceName(
      kNewName, [&validation_result](DeviceNameValidationResult result) {
        validation_result = result;
      });
  EXPECT_EQ(validation_result, DeviceNameValidationResult::kValid);
}

TEST_F(NearbyShareSettingsTest, SetDataUsageLogsAnalyticsOnlyWhenChanged) {
  DataUsage initial_usage = settings()->GetDataUsage();

  // Setting the current data usage value again must not emit analytics.
  EXPECT_CALL(analytics_recorder_, NewSetDataUsage).Times(0);
  settings()->SetDataUsage(initial_usage);
  testing::Mock::VerifyAndClearExpectations(&analytics_recorder_);

  // Changing data usage to a different value must emit NewSetDataUsage once.
  DataUsage updated_usage = initial_usage == DataUsage::OFFLINE_DATA_USAGE
                                ? DataUsage::ONLINE_DATA_USAGE
                                : DataUsage::OFFLINE_DATA_USAGE;
  EXPECT_CALL(analytics_recorder_,
              NewSetDataUsage(initial_usage, updated_usage))
      .Times(1);
  settings()->SetDataUsage(updated_usage);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
