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

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/analytics/event_logger.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/clock.h"
#include "internal/platform/device_info.h"
#include "internal/platform/mutex_lock.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::location::nearby::proto::sharing::DesktopNotification;
using ::location::nearby::proto::sharing::DesktopTransferEventType;
using ::location::nearby::proto::sharing::ShowNotificationStatus;
using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::proto::DataUsage;
using ::nearby::sharing::proto::DeviceVisibility;
using ::nearby::sharing::proto::FastInitiationNotificationState;

constexpr absl::string_view kPreferencesObserverName =
    "nearby-sharing-settings";
constexpr int kMaxVisibilityExpirationSeconds =
    prefs::kDefaultMaxVisibilityExpirationSeconds;

ShowNotificationStatus GetNotificationStatus(
    FastInitiationNotificationState state) {
  switch (state) {
    case FastInitiationNotificationState::ENABLED_FAST_INIT:
      return ShowNotificationStatus::SHOW;
    case FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT:
    case FastInitiationNotificationState::DISABLED_BY_FEATURE_FAST_INIT:
      return ShowNotificationStatus::NOT_SHOW;
    default:
      return ShowNotificationStatus::UNKNOWN_SHOW_NOTIFICATION_STATUS;
  }
}
}  // namespace

NearbyShareSettings::NearbyShareSettings(
    Context* context,
    nearby::Clock* clock,
    nearby::DeviceInfo& device_info,
    PreferenceManager& preference_manager,
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    nearby::analytics::EventLogger* event_logger)
    : clock_(clock),
      device_info_(device_info),
      preference_manager_(preference_manager),
      local_device_data_manager_(local_device_data_manager),
      analytics_recorder_(
          std::make_unique<analytics::AnalyticsRecorder>(event_logger)) {
  is_desctructing_ = std::make_shared<bool>(false);
  visibility_expiration_timer_ = context->CreateTimer();
  RestoreFallbackVisibility();
  preference_manager_.AddObserver(
      kPreferencesObserverName,
      [this, desctructing =
                 std::weak_ptr<bool>(is_desctructing_)](absl::string_view key) {
        std::shared_ptr<bool> is_desctructing = desctructing.lock();
        if (is_desctructing == nullptr || *is_desctructing) {
          NL_LOG(WARNING) << ": Ignore the preferences change callback.";
          return;
        }

        OnPreferenceChanged(key);
      });
  local_device_data_manager_->AddObserver(this);
}

NearbyShareSettings::~NearbyShareSettings() {
  MutexLock lock(&mutex_);
  is_desctructing_ = nullptr;
  preference_manager_.RemoveObserver(kPreferencesObserverName);
  local_device_data_manager_->RemoveObserver(this);
  visibility_expiration_timer_->Stop();
}

bool NearbyShareSettings::GetEnabled() const {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(prefs::kNearbySharingEnabledName,
                                          false);
}

FastInitiationNotificationState
NearbyShareSettings::GetFastInitiationNotificationState() const {
  MutexLock lock(&mutex_);
  return static_cast<FastInitiationNotificationState>(
      preference_manager_.GetInteger(
          prefs::kNearbySharingFastInitiationNotificationStateName,
          static_cast<int>(
              FastInitiationNotificationState::ENABLED_FAST_INIT)));
}

void NearbyShareSettings::SetIsFastInitiationHardwareSupported(
    bool is_supported) {
  MutexLock lock(&mutex_);

  // If the new value is the same as the old value, don't notify observers.
  if (is_fast_initiation_hardware_supported_ == is_supported) {
    return;
  }

  is_fast_initiation_hardware_supported_ = is_supported;

  for (Observer* observer : observers_set_.GetObservers()) {
    observer->OnIsFastInitiationHardwareSupportedChanged(is_supported);
  }
}

std::string NearbyShareSettings::GetDeviceName() const {
  return local_device_data_manager_->GetDeviceName();
}

DataUsage NearbyShareSettings::GetDataUsage() const {
  MutexLock lock(&mutex_);
  return static_cast<DataUsage>(
      preference_manager_.GetInteger(prefs::kNearbySharingDataUsageName, 0));
}

void NearbyShareSettings::StartVisibilityTimer(
    absl::Duration expiration) const {
  NL_LOG(INFO) << __func__
               << ": start visibility timer. expiration=" << expiration;
  visibility_expiration_timer_->Start(
      expiration / absl::Milliseconds(1), 0, [this]() {
        NL_LOG(INFO) << __func__ << ": visibility timer expired.";
        int visibility;
        {
          MutexLock lock(&mutex_);
          visibility_expiration_timer_->Stop();
          visibility = preference_manager_.GetInteger(
              prefs::kNearbySharingBackgroundFallbackVisibilityName,
              static_cast<int>(prefs::kDefaultFallbackVisibility));
        }
        SetVisibility(DeviceVisibility(visibility));
      });
}

void NearbyShareSettings::RestoreFallbackVisibility() {
  MutexLock lock(&mutex_);

  int64_t expiration_seconds = preference_manager_.GetInteger(
      prefs::kNearbySharingBackgroundVisibilityExpirationSeconds, 0);
  int64_t fallback_visibility = preference_manager_.GetInteger(
      prefs::kNearbySharingBackgroundFallbackVisibilityName,
      static_cast<int>(prefs::kDefaultFallbackVisibility));
  fallback_visibility_ = static_cast<DeviceVisibility>(fallback_visibility);

  int64_t now_seconds = absl::ToUnixSeconds(clock_->Now());
  int64_t remaining_seconds = expiration_seconds - now_seconds;
  int64_t diff = kMaxVisibilityExpirationSeconds - remaining_seconds;
  NL_LOG(INFO) << __func__ << ": diff=" << diff << ", now=" << now_seconds
               << ", expiration=" << expiration_seconds
               << ", max=" << kMaxVisibilityExpirationSeconds;
  if (remaining_seconds > 0 &&
      remaining_seconds <= kMaxVisibilityExpirationSeconds) {  // Not expired
    StartVisibilityTimer(absl::Seconds(remaining_seconds));
  } else if (expiration_seconds != 0) {  // Expired.
    NL_LOG(INFO) << __func__
                 << ": timer is already expired. Restore fallback visibility.";
    SetVisibility(static_cast<DeviceVisibility>(fallback_visibility));
  } else {
    NL_LOG(INFO) << __func__ << ": No running fallback Visibility.";
  }
}

std::vector<std::string> NearbyShareSettings::GetAllowedContacts() const {
  MutexLock lock(&mutex_);
  std::vector<std::string> allowed_contacts =
      preference_manager_.GetStringArray(
          prefs::kNearbySharingAllowedContactsName, {});
  return allowed_contacts;
}

bool NearbyShareSettings::IsOnboardingComplete() const {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(
      prefs::kNearbySharingOnboardingCompleteName, false);
}

std::string NearbyShareSettings::GetCustomSavePath() const {
  MutexLock lock(&mutex_);
  return preference_manager_.GetString(
      prefs::kNearbySharingCustomSavePath,
      GetCompatibleU8String(device_info_.GetDownloadPath().u8string()));
}

bool NearbyShareSettings::IsDisabledByPolicy() const { return !GetEnabled(); }

void NearbyShareSettings::AddSettingsObserver(Observer* observer) {
  MutexLock lock(&mutex_);
  observers_set_.AddObserver(observer);
}

void NearbyShareSettings::RemoveSettingsObserver(Observer* observer) {
  MutexLock lock(&mutex_);
  observers_set_.RemoveObserver(observer);
}

void NearbyShareSettings::GetEnabled(std::function<void(bool)> callback) {
  std::move(callback)(GetEnabled());
}

void NearbyShareSettings::GetFastInitiationNotificationState(
    std::function<void(FastInitiationNotificationState)> callback) {
  std::move(callback)(GetFastInitiationNotificationState());
}

void NearbyShareSettings::GetIsFastInitiationHardwareSupported(
    std::function<void(bool)> callback) {
  MutexLock lock(&mutex_);
  std::move(callback)(is_fast_initiation_hardware_supported_);
}

void NearbyShareSettings::SetEnabled(bool enabled) {
  MutexLock lock(&mutex_);

  preference_manager_.SetBoolean(prefs::kNearbySharingEnabledName, enabled);
  if (enabled &&
      GetVisibility() == DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED) {
    NL_LOG(ERROR) << "Nearby Share enabled with visibility unset. Setting "
                     "default visibility to kEveryone.";
    SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_EVERYONE);
  }
}

void NearbyShareSettings::SetFastInitiationNotificationState(
    FastInitiationNotificationState state) {
  MutexLock lock(&mutex_);
  analytics_recorder_->NewToggleShowNotification(
      GetNotificationStatus(GetFastInitiationNotificationState()),
      GetNotificationStatus(state));

  preference_manager_.SetInteger(
      prefs::kNearbySharingFastInitiationNotificationStateName,
      static_cast<int>(state));
}

void NearbyShareSettings::IsOnboardingComplete(
    std::function<void(bool)> callback) {
  std::move(callback)(IsOnboardingComplete());
}

void NearbyShareSettings::SetIsOnboardingComplete(
    bool completed, std::function<void()> callback) {
  MutexLock lock(&mutex_);
  preference_manager_.SetBoolean(prefs::kNearbySharingOnboardingCompleteName,
                                   completed);
  std::move(callback)();
}

void NearbyShareSettings::GetDeviceName(
    std::function<void(absl::string_view)> callback) {
  std::move(callback)(GetDeviceName());
}

void NearbyShareSettings::ValidateDeviceName(
    absl::string_view device_name,
    std::function<void(DeviceNameValidationResult)> callback) {
  std::move(callback)(
      local_device_data_manager_->ValidateDeviceName(device_name));
}

void NearbyShareSettings::SetDeviceName(
    absl::string_view device_name,
    std::function<void(DeviceNameValidationResult)> callback) {
  analytics_recorder_->NewSetDeviceName(device_name.size());
  std::move(callback)(local_device_data_manager_->SetDeviceName(device_name));
}

void NearbyShareSettings::GetDataUsage(
    std::function<void(DataUsage)> callback) {
  std::move(callback)(GetDataUsage());
}

void NearbyShareSettings::SetDataUsage(DataUsage data_usage) {
  MutexLock lock(&mutex_);
  analytics_recorder_->NewSetDataUsage(GetDataUsage(), data_usage);
  preference_manager_.SetInteger(prefs::kNearbySharingDataUsageName,
                                   static_cast<int>(data_usage));
}

void NearbyShareSettings::GetVisibility(
    std::function<void(DeviceVisibility)> callback) {
  std::move(callback)(GetVisibility());
}

DeviceVisibility NearbyShareSettings::GetVisibility() const {
  MutexLock lock(&mutex_);
  DeviceVisibility visibility =
      static_cast<DeviceVisibility>(preference_manager_.GetInteger(
          prefs::kNearbySharingBackgroundVisibilityName,
          static_cast<int>(prefs::kDefaultVisibility)));
  if (visibility == DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS) {
    // Set the visibility to self share if it's only visible to selected
    // contacts, as part of QuickShare rebrand work.
    SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE);
    return DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE;
  }
  return visibility;
}

void NearbyShareSettings::SetVisibility(DeviceVisibility visibility,
                                        absl::Duration expiration) const {
  MutexLock lock(&mutex_);
  DeviceVisibility last_visibility =
      static_cast<DeviceVisibility>(preference_manager_.GetInteger(
          prefs::kNearbySharingBackgroundVisibilityName,
          static_cast<int>(prefs::kDefaultVisibility)));
  analytics_recorder_->NewSetVisibility(last_visibility, visibility,
                                        expiration / absl::Milliseconds(1));

  NL_VLOG(1) << __func__
             << ": set visibility. visibility=" << static_cast<int>(visibility)
             << ", expiration=" << expiration;
  if (visibility_expiration_timer_->IsRunning()) {
    NL_VLOG(1) << __func__
               << ": temporary visibility timer is running. stopped.";
    visibility_expiration_timer_->Stop();
  }

  absl::Time now = clock_->Now();
  if (expiration != absl::ZeroDuration()) {
    NL_VLOG(1) << __func__ << ": temporary visibility timer starts.";
    absl::Time fallback_visibility_timestamp = now + expiration;
    preference_manager_.SetInteger(
        prefs::kNearbySharingBackgroundVisibilityExpirationSeconds,
        absl::ToUnixSeconds(fallback_visibility_timestamp));
    StartVisibilityTimer(expiration);
  } else {
    preference_manager_.SetInteger(
        prefs::kNearbySharingBackgroundVisibilityExpirationSeconds, 0);
  }

  last_visibility_timestamp_ = now;
  last_visibility_ = last_visibility;
  preference_manager_.SetInteger(
      prefs::kNearbySharingBackgroundVisibilityName,
      static_cast<int>(visibility));
}

absl::Time NearbyShareSettings::GetLastVisibilityTimestamp() const {
  MutexLock lock(&mutex_);
  return last_visibility_timestamp_;
}

proto::DeviceVisibility NearbyShareSettings::GetLastVisibility() const {
  MutexLock lock(&mutex_);
  return static_cast<proto::DeviceVisibility>(last_visibility_);
}

DeviceVisibility NearbyShareSettings::GetFallbackVisibility() const {
  MutexLock lock(&mutex_);
  NL_VLOG(1) << __func__ << ": get fallback visibility called.";
  return fallback_visibility_.has_value() ? *fallback_visibility_
                                          : prefs::kDefaultFallbackVisibility;
}

void NearbyShareSettings::SetFallbackVisibility(
    DeviceVisibility visibility) const {
  MutexLock lock(&mutex_);
  NL_VLOG(1) << __func__ << ": set fallback visibility. visibility="
             << static_cast<int>(visibility);
  if (visibility == DeviceVisibility::DEVICE_VISIBILITY_EVERYONE) {
    NL_VLOG(1) << __func__ << ": visibility is everyone. Skip.";
    return;
  }

  fallback_visibility_ = visibility;
  preference_manager_.SetInteger(
      prefs::kNearbySharingBackgroundFallbackVisibilityName,
      static_cast<int>(visibility));
}

bool NearbyShareSettings::GetIsTemporarilyVisible() const {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(
      prefs::kNearbySharingBackgroundTemporarilyVisibleName, false);
}

void NearbyShareSettings::SetIsTemporarilyVisible(
    bool is_temporarily_visible) const {
  MutexLock lock(&mutex_);
  preference_manager_.SetBoolean(
      prefs::kNearbySharingBackgroundTemporarilyVisibleName,
      is_temporarily_visible);
}

void NearbyShareSettings::GetAllowedContacts(
    std::function<void(absl::Span<const std::string>)> callback) {
  std::move(callback)(GetAllowedContacts());
}

void NearbyShareSettings::SetAllowedContacts(
    absl::Span<const std::string> allowed_contacts) {
  MutexLock lock(&mutex_);
  preference_manager_.SetStringArray(prefs::kNearbySharingAllowedContactsName,
                                       allowed_contacts);
}

void NearbyShareSettings::GetCustomSavePathAsync(
    const std::function<void(absl::string_view)>& callback) const {
  callback(GetCustomSavePath());
}

void NearbyShareSettings::SetCustomSavePathAsync(
    absl::string_view save_path, const std::function<void()>& callback) {
  MutexLock lock(&mutex_);
  preference_manager_.SetString(prefs::kNearbySharingCustomSavePath,
                                  save_path);
  callback();
}

void NearbyShareSettings::OnPreferenceChanged(absl::string_view key) {
  MutexLock lock(&mutex_);
  if (key == prefs::kNearbySharingEnabledName) {
    NotifyAllObservers(key, Observer::Data(GetEnabled()));

    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_sharing_feature::
                kEnableBackgroundScanning)) {
      ProcessFastInitiationNotificationParentPrefChanged(GetEnabled());
    }
  } else if (key == prefs::kNearbySharingFastInitiationNotificationStateName) {
    NotifyAllObservers(key, Observer::Data(static_cast<int64_t>(
                                GetFastInitiationNotificationState())));
  } else if (key == prefs::kNearbySharingBackgroundVisibilityName) {
    NotifyAllObservers(key,
                       Observer::Data(static_cast<int64_t>(GetVisibility())));
  } else if (key == prefs::kNearbySharingDataUsageName) {
    NotifyAllObservers(key,
                       Observer::Data(static_cast<int64_t>(GetDataUsage())));
  } else if (key == prefs::kNearbySharingAllowedContactsName) {
    NotifyAllObservers(key, Observer::Data(GetAllowedContacts()));
  } else if (key == prefs::kNearbySharingOnboardingCompleteName) {
    NotifyAllObservers(key, Observer::Data(IsOnboardingComplete()));
  } else if (key == prefs::kNearbySharingIsReceivingName) {
    NotifyAllObservers(key, Observer::Data(GetIsReceiving()));
  } else if (key == prefs::kNearbySharingCustomSavePath) {
    NotifyAllObservers(key, Observer::Data(GetCustomSavePath()));
  } else {
    // Not a monitored key.
    return;
  }
}

void NearbyShareSettings::OnLocalDeviceDataChanged(bool did_device_name_change,
                                                   bool did_full_name_change,
                                                   bool did_icon_url_change) {
  MutexLock lock(&mutex_);
  if (!did_device_name_change) return;

  std::string device_name = GetDeviceName();
  NotifyAllObservers(prefs::kNearbySharingDeviceNameName,
                     Observer::Data(device_name));
}

void NearbyShareSettings::NotifyAllObservers(absl::string_view key,
                                             Observer::Data value) {
  for (Observer* observer : observers_set_.GetObservers()) {
    observer->OnSettingChanged(key, value);
  }
}

void NearbyShareSettings::ProcessFastInitiationNotificationParentPrefChanged(
    bool enabled) {
  // If onboarding is not yet complete the Nearby feature should not be able
  // to affect the enabled state.
  if (!IsOnboardingComplete()) {
    return;
  }

  // If the user explicitly disabled notifications, toggling the Nearby Share
  // feature does not re-enable the notification sub-feature.
  if (GetFastInitiationNotificationState() ==
      FastInitiationNotificationState::DISABLED_BY_USER_FAST_INIT) {
    return;
  }
  SetFastInitiationNotificationState(
      enabled ? FastInitiationNotificationState::ENABLED_FAST_INIT
              : FastInitiationNotificationState::DISABLED_BY_FEATURE_FAST_INIT);
}

bool NearbyShareSettings::GetIsReceiving() {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(prefs::kNearbySharingIsReceivingName,
                                          true);
}

void NearbyShareSettings::SetIsReceiving(bool is_receiving) const {
  MutexLock lock(&mutex_);
  preference_manager_.SetBoolean(prefs::kNearbySharingIsReceivingName,
                                   is_receiving);
}

bool NearbyShareSettings::GetIsAnalyticsEnabled() {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(
      prefs::kNearbySharingIsAnalyticsEnabledName, true);
}

void NearbyShareSettings::SetIsAnalyticsEnabled(
    bool is_analytics_enabled) const {
  MutexLock lock(&mutex_);
  preference_manager_.SetBoolean(prefs::kNearbySharingIsAnalyticsEnabledName,
                                   is_analytics_enabled);
}

std::string NearbyShareSettings::Dump() const {
  std::stringstream sstream;
  sstream << "Nearby Share Settings" << std::endl;
  sstream << "  Device name: " << GetDeviceName() << std::endl;
  sstream << "  Visibility: " << DeviceVisibility_Name(GetVisibility())
          << std::endl;
  sstream << "  Enabled: " << std::boolalpha << GetEnabled() << std::noboolalpha
          << std::endl;
  sstream << "  FastInitiationNotification: "
          << FastInitiationNotificationState_Name(
                 GetFastInitiationNotificationState())
          << std::endl;
  sstream << "  DataUsage: " << DataUsage_Name(GetDataUsage()) << std::endl;
  sstream << "  Last Visibility: " << DeviceVisibility_Name(GetLastVisibility())
          << std::endl;
  return sstream.str();
}

bool NearbyShareSettings::GetIsAllContactsEnabled() {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(
      prefs::kNearbySharingIsAllContactsEnabledName, true);
}

void NearbyShareSettings::SetIsAllContactsEnabled(
    bool is_all_contacts_enabled) const {
  MutexLock lock(&mutex_);
  preference_manager_.SetBoolean(
      prefs::kNearbySharingIsAllContactsEnabledName, is_all_contacts_enabled);

  if (is_all_contacts_enabled) {
    if (GetVisibility() ==
        DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS) {
      SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS);
    }
  } else {
    if (GetVisibility() == DeviceVisibility::DEVICE_VISIBILITY_ALL_CONTACTS) {
      SetVisibility(DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS);
    }
  }
}

bool NearbyShareSettings::GetAutoAppStartEnabled() const {
  MutexLock lock(&mutex_);
  return preference_manager_.GetBoolean(
      prefs::kNearbySharingAutoAppStartEnabledName, true);
}

void NearbyShareSettings::SetAutoAppStartEnabled(bool is_auto_app_start) const {
  MutexLock lock(&mutex_);
  preference_manager_.SetBoolean(prefs::kNearbySharingAutoAppStartEnabledName,
                                   is_auto_app_start);
}

void NearbyShareSettings::SendDesktopNotification(
    DesktopNotification event) const {
  analytics_recorder_->NewSendDesktopNotification(event);
}

void NearbyShareSettings::SendDesktopTransferEvent(
    DesktopTransferEventType event) const {
  analytics_recorder_->NewSendDesktopTransferEvent(event);
}

bool NearbyShareSettings::is_fast_initiation_hardware_supported() {
  MutexLock lock(&mutex_);
  return is_fast_initiation_hardware_supported_;
}

}  // namespace sharing
}  // namespace nearby
