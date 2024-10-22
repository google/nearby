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
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "internal/platform/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/common/compatible_u8_string.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {
namespace {

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
    Context* context, nearby::Clock* clock, nearby::DeviceInfo& device_info,
    PreferenceManager& preference_manager,
    NearbyShareLocalDeviceDataManager* local_device_data_manager,
    analytics::AnalyticsRecorder* analytics_recorder)
    : context_(context),
      clock_(clock),
      device_info_(device_info),
      preference_manager_(preference_manager),
      local_device_data_manager_(local_device_data_manager),
      analytics_recorder_(analytics_recorder),
      fallback_visibility_(prefs::kDefaultFallbackVisibility) {
  is_desctructing_ = std::make_shared<bool>(false);
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
  absl::MutexLock lock(&mutex_);
  is_desctructing_ = nullptr;
  preference_manager_.RemoveObserver(kPreferencesObserverName);
  local_device_data_manager_->RemoveObserver(this);
}

FastInitiationNotificationState
NearbyShareSettings::GetFastInitiationNotificationState() const {
  return static_cast<FastInitiationNotificationState>(
      preference_manager_.GetInteger(
          prefs::kNearbySharingFastInitiationNotificationStateName,
          static_cast<int>(
              FastInitiationNotificationState::ENABLED_FAST_INIT)));
}

void NearbyShareSettings::SetIsFastInitiationHardwareSupported(
    bool is_supported) {
  {
    absl::MutexLock lock(&mutex_);
    // If the new value is the same as the old value, don't notify observers.
    if (is_fast_initiation_hardware_supported_ == is_supported) {
      return;
    }
    is_fast_initiation_hardware_supported_ = is_supported;
  }
  for (Observer* observer : observers_set_.GetObservers()) {
    observer->OnIsFastInitiationHardwareSupportedChanged(is_supported);
  }
}

std::string NearbyShareSettings::GetDeviceName() const {
  return local_device_data_manager_->GetDeviceName();
}

DataUsage NearbyShareSettings::GetDataUsage() const {
  return static_cast<DataUsage>(
      preference_manager_.GetInteger(prefs::kNearbySharingDataUsageName, 0));
}

void NearbyShareSettings::StartVisibilityTimer(
    absl::Duration expiration) {
  NL_LOG(INFO) << __func__
               << ": start visibility timer. expiration=" << expiration;
  visibility_expiration_timer_ = std::make_unique<ThreadTimer>(
      *context_->GetTaskRunner(), "nearby_share_settings_visibility_timer",
      expiration, [this]() {
        NL_LOG(INFO) << __func__ << ": visibility timer expired.";
        proto::DeviceVisibility visibility;
        {
          absl::MutexLock lock(&mutex_);
          visibility = fallback_visibility_;
          visibility_expiration_timer_.reset();
        }
        SetVisibility(visibility);
      });
}

void NearbyShareSettings::RestoreFallbackVisibility() {
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

std::string NearbyShareSettings::GetCustomSavePath() const {
  return preference_manager_.GetString(
      prefs::kNearbySharingCustomSavePath,
      GetCompatibleU8String(device_info_.GetDownloadPath().u8string()));
}

bool NearbyShareSettings::IsDisabledByPolicy() const { return false; }

void NearbyShareSettings::AddSettingsObserver(Observer* observer) {
  observers_set_.AddObserver(observer);
}

void NearbyShareSettings::RemoveSettingsObserver(Observer* observer) {
  observers_set_.RemoveObserver(observer);
}

void NearbyShareSettings::SetFastInitiationNotificationState(
    FastInitiationNotificationState state) {
  if (analytics_recorder_ != nullptr) {
    analytics_recorder_->NewToggleShowNotification(
        GetNotificationStatus(GetFastInitiationNotificationState()),
        GetNotificationStatus(state));
  }

  preference_manager_.SetInteger(
      prefs::kNearbySharingFastInitiationNotificationStateName,
      static_cast<int>(state));
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
  if (analytics_recorder_ != nullptr) {
    analytics_recorder_->NewSetDeviceName(device_name.size());
  }
  std::move(callback)(local_device_data_manager_->SetDeviceName(device_name));
}

void NearbyShareSettings::SetDataUsage(DataUsage data_usage) {
  if (analytics_recorder_ != nullptr) {
    analytics_recorder_->NewSetDataUsage(GetDataUsage(), data_usage);
  }
  preference_manager_.SetInteger(prefs::kNearbySharingDataUsageName,
                                 static_cast<int>(data_usage));
}

DeviceVisibility NearbyShareSettings::GetVisibility() const {
  DeviceVisibility visibility =
      static_cast<DeviceVisibility>(preference_manager_.GetInteger(
          prefs::kNearbySharingBackgroundVisibilityName,
          static_cast<int>(prefs::kDefaultVisibility)));
  if (visibility == DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS) {
    // Set the visibility to self share if it's only visible to selected
    // contacts, as part of QuickShare rebrand work.
    visibility = DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE;
  }
  return visibility;
}

void NearbyShareSettings::SetVisibility(DeviceVisibility visibility,
                                        absl::Duration expiration) {
  absl::MutexLock lock(&mutex_);
  if (visibility == DeviceVisibility::DEVICE_VISIBILITY_SELECTED_CONTACTS) {
    // This should really be an error, but this function does not return errors,
    // so change it to self share as in GetVisibility().
    visibility = DeviceVisibility::DEVICE_VISIBILITY_SELF_SHARE;
  }
  DeviceVisibility last_visibility = GetVisibility();
  absl::Duration max_expiration =
      absl::Seconds(kMaxVisibilityExpirationSeconds);
  if (expiration > max_expiration) {
    LOG(WARNING) << "Set visbility expiration is too long. visibility="
                 << static_cast<int>(visibility)
                 << ", expiration=" << expiration;
    expiration = max_expiration;
  }
  if (analytics_recorder_ != nullptr) {
    analytics_recorder_->NewSetVisibility(
        last_visibility, visibility, absl::ToInt64Milliseconds(expiration));
  }

  NL_VLOG(1) << __func__
             << ": set visibility. visibility=" << static_cast<int>(visibility)
             << ", expiration=" << expiration;
  visibility_expiration_timer_.reset();

  SetFallbackVisibility(last_visibility);
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
  preference_manager_.SetInteger(prefs::kNearbySharingBackgroundVisibilityName,
                                 static_cast<int>(visibility));
}

absl::Time NearbyShareSettings::GetLastVisibilityTimestamp() const {
  absl::MutexLock lock(&mutex_);
  return last_visibility_timestamp_;
}

proto::DeviceVisibility NearbyShareSettings::GetLastVisibility() const {
  absl::MutexLock lock(&mutex_);
  return static_cast<proto::DeviceVisibility>(last_visibility_);
}

NearbyShareSettings::FallbackVisibilityInfo
NearbyShareSettings::GetRawFallbackVisibility() const {
  return {
    .visibility = fallback_visibility_,
    .fallback_time = absl::FromUnixSeconds(preference_manager_.GetInteger(
        prefs::kNearbySharingBackgroundVisibilityExpirationSeconds, 0))
  };
}

NearbyShareSettings::FallbackVisibilityInfo
NearbyShareSettings::GetFallbackVisibility() const {
  absl::MutexLock lock(&mutex_);
  FallbackVisibilityInfo result{
      .visibility = DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED,
      .fallback_time = absl::UnixEpoch(),
  };
  // Check if visibility is temporary.
  if (visibility_expiration_timer_ != nullptr &&
      visibility_expiration_timer_->IsRunning()) {
    result = GetRawFallbackVisibility();
  }
  NL_VLOG(1) << __func__ << ": get fallback visibility "
             << static_cast<int>(result.visibility)
             << " expiration: " << result.fallback_time;
  return result;
}

void NearbyShareSettings::SetFallbackVisibility(DeviceVisibility visibility) {
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

void NearbyShareSettings::SetCustomSavePathAsync(
    absl::string_view save_path, const std::function<void()>& callback) {
  absl::MutexLock lock(&mutex_);
  preference_manager_.SetString(prefs::kNearbySharingCustomSavePath, save_path);
  callback();
}

void NearbyShareSettings::OnPreferenceChanged(absl::string_view key) {
  if (key == prefs::kNearbySharingFastInitiationNotificationStateName) {
    NotifyAllObservers(key, Observer::Data(static_cast<int64_t>(
                                GetFastInitiationNotificationState())));
  } else if (key == prefs::kNearbySharingBackgroundVisibilityName) {
    NotifyAllObservers(key,
                       Observer::Data(static_cast<int64_t>(GetVisibility())));
  } else if (key == prefs::kNearbySharingDataUsageName) {
    NotifyAllObservers(key,
                       Observer::Data(static_cast<int64_t>(GetDataUsage())));
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

bool NearbyShareSettings::GetIsAnalyticsEnabled() const {
  return preference_manager_.GetBoolean(
      prefs::kNearbySharingIsAnalyticsEnabledName, true);
}

void NearbyShareSettings::SetIsAnalyticsEnabled(bool is_analytics_enabled) {
  preference_manager_.SetBoolean(prefs::kNearbySharingIsAnalyticsEnabledName,
                                 is_analytics_enabled);
}

std::string NearbyShareSettings::Dump() const {
  std::stringstream sstream;
  sstream << "Nearby Share Settings" << std::endl;
  sstream << "  Device name: " << GetDeviceName() << std::endl;
  sstream << "  Visibility: " << DeviceVisibility_Name(GetVisibility())
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

bool NearbyShareSettings::is_fast_initiation_hardware_supported() {
  absl::MutexLock lock(&mutex_);
  return is_fast_initiation_hardware_supported_;
}

}  // namespace sharing
}  // namespace nearby
