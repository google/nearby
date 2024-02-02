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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SETTINGS_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SETTINGS_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/analytics/event_logger.h"
#include "internal/base/observer_list.h"
#include "internal/platform/clock.h"
#include "internal/platform/device_info.h"
#include "internal/platform/mutex.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/settings_observer_data.pb.h"

namespace nearby {
namespace sharing {

// Provides a type safe wrapper/abstraction over prefs for both C++ and
// Javascript (over mojo) to interact with Nearby user settings. This class
// always reads directly from prefs and relies on preference's memory cache.
// It is designed to be contained within the Nearby Sharing Service with an
// instance per user profile. This class also helps to keep some prefs
// logic out of |NearbyShareServiceImpl|.
//
// This class is also used to expose device properties that affect the settings
// UI, but cannot be added at load time because they need to be re-computed. See
// GetIsFastInitiationHardwareSupported() as an example.
//
// The mojo interface is intended to be exposed in settings, os_settings, and
// the nearby WebUI.
//
// NOTE: The pref-change registrar only notifies observers of pref value
// changes; observers are not notified if the pref value is set but does not
// change. This class inherits this behavior.
//
// NOTE: Because the observer interface is over mojo, setting a value directly
// will not synchronously trigger the observer event. Generally this is not a
// problem because these settings should only be changed by user interaction,
// but this is necessary to know when writing unit-tests.
class NearbyShareSettings
    : nearby::sharing::NearbyShareLocalDeviceDataManager::Observer {
 public:
  class Observer {
   public:
    // LINT.IfChange(TaggedUnion)
    // The C++ counterpart of message `Data` in
    // third_party/nearby/sharing/proto/settings_observer_data.proto
    struct Data {
      proto::Tag tag;

      union Value {
        bool as_bool;
        int64_t as_int64;
        std::string as_string;
        std::vector<std::string> as_string_array;

        explicit Value(std::nullptr_t = nullptr) {}
        explicit Value(bool data) : as_bool(data) {}
        explicit Value(int64_t data) : as_int64(data) {}
        explicit Value(std::string data) : as_string(data) {}
        explicit Value(std::vector<std::string> data) : as_string_array(data) {}

        ~Value() {}
      } value;

      ~Data() {
        // Call the destructors of members who are not basic types.
        if (tag == proto::Tag::TAG_STRING) {
          this->value.as_string.~basic_string();
        }
        if (tag == proto::Tag::TAG_STRING_ARRAY) {
          this->value.as_string_array.~vector();
        }
      }

      explicit Data(std::nullptr_t = nullptr)
          : tag(proto::Tag::TAG_NULL), value() {}

      explicit Data(const bool data) : tag(proto::Tag::TAG_BOOL), value(data) {}

      explicit Data(const int64_t data)
          : tag(proto::Tag::TAG_INT64), value(data) {}

      explicit Data(const std::string& data)
          : tag(proto::Tag::TAG_STRING), value(data) {}

      explicit Data(const std::vector<std::string>& data)
          : tag(proto::Tag::TAG_STRING_ARRAY), value(data) {}

      explicit operator std::unique_ptr<proto::Data>() const {
        auto result = std::make_unique<proto::Data>();
        result->set_tag(this->tag);
        switch (this->tag) {
          case proto::Tag::TAG_NULL:
            break;
          case proto::Tag::TAG_BOOL:
            result->set_as_bool(this->value.as_bool);
            break;
          case proto::Tag::TAG_INT64:
            result->set_as_int64(this->value.as_int64);
            break;
          case proto::Tag::TAG_STRING:
            result->set_as_string(this->value.as_string);
            break;
          case proto::Tag::TAG_STRING_ARRAY:
            for (const auto& value : this->value.as_string_array) {
              result->add_as_string_array(value);
            }
            break;
          default:
            LOG(FATAL) << "Invalid tag: " << this->tag;
            break;
        }
        return result;
      }
    };
    // LINT.ThenChange(
    //   //depot/google3/third_party/nearby/sharing/proto/settings_observer_data.proto:TaggedUnion
    // )

    virtual ~Observer() = default;

    virtual void OnSettingChanged(absl::string_view key, const Data& data) {}
    // Called when the fast initiation hardware offloading support state
    // changes.
    virtual void OnIsFastInitiationHardwareSupportedChanged(
        bool is_supported) = 0;
  };

  NearbyShareSettings(
      Context* context,
      nearby::Clock* clock,
      nearby::DeviceInfo& device_info,
      nearby::sharing::api::PreferenceManager& preference_manager,
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
      nearby::analytics::EventLogger* event_logger = nullptr);
  ~NearbyShareSettings() override;

  // Internal synchronous getters for C++ clients
  bool GetEnabled() const;
  proto::FastInitiationNotificationState GetFastInitiationNotificationState()
      const;
  bool is_fast_initiation_hardware_supported();
  void SetIsFastInitiationHardwareSupported(bool is_supported);
  std::string GetDeviceName() const;
  proto::DataUsage GetDataUsage() const;
  proto::DeviceVisibility GetVisibility() const;
  // Gets the timestamp of last visibility change. Need the timestamp to decide
  // whether need to send optional signature data during key pairing.
  absl::Time GetLastVisibilityTimestamp() const;
  proto::DeviceVisibility GetLastVisibility() const;

  proto::DeviceVisibility GetFallbackVisibility() const;
  bool GetIsTemporarilyVisible() const;
  void SetIsTemporarilyVisible(bool is_temporarily_visible) const;
  std::vector<std::string> GetAllowedContacts() const;
  bool IsOnboardingComplete() const;
  std::string GetCustomSavePath() const;

  // Returns true if the feature is disabled by policy.
  bool IsDisabledByPolicy() const;

  // Asynchronous APIs exposed by NearbyShareSettings
  void AddSettingsObserver(Observer* observer);
  void RemoveSettingsObserver(Observer* observer);
  void GetEnabled(std::function<void(bool)> callback);
  void GetFastInitiationNotificationState(
      std::function<void(proto::FastInitiationNotificationState)> callback);
  void GetIsFastInitiationHardwareSupported(std::function<void(bool)> callback);
  void SetEnabled(bool enabled);
  void SetFastInitiationNotificationState(
      proto::FastInitiationNotificationState state);
  void IsOnboardingComplete(std::function<void(bool)> callback);
  void SetIsOnboardingComplete(bool completed, std::function<void()> callback);
  void GetDeviceName(std::function<void(absl::string_view)> callback);
  void ValidateDeviceName(
      absl::string_view device_name,
      std::function<void(DeviceNameValidationResult)> callback);
  void SetDeviceName(absl::string_view device_name,
                     std::function<void(DeviceNameValidationResult)> callback);
  void GetDataUsage(std::function<void(proto::DataUsage)> callback);
  void SetDataUsage(proto::DataUsage data_usage);
  void GetVisibility(std::function<void(proto::DeviceVisibility)> callback);
  void SetVisibility(proto::DeviceVisibility visibility,
                     absl::Duration expiration = absl::ZeroDuration()) const;
  void SetFallbackVisibility(proto::DeviceVisibility visibility) const;
  bool GetIsReceiving();
  void SetIsReceiving(bool is_receiving) const;
  bool GetIsAnalyticsEnabled();
  void SetIsAnalyticsEnabled(bool is_analytics_enabled) const;
  bool GetIsAllContactsEnabled();
  void SetIsAllContactsEnabled(bool is_all_contacts_enabled) const;

  void GetAllowedContacts(
      std::function<void(absl::Span<const std::string>)> callback);
  void SetAllowedContacts(absl::Span<const std::string> allowed_contacts);

  void GetCustomSavePathAsync(
      const std::function<void(absl::string_view)>& callback) const;
  void SetCustomSavePathAsync(absl::string_view save_path,
                              const std::function<void()>& callback);

  bool GetAutoAppStartEnabled() const;
  void SetAutoAppStartEnabled(bool is_auto_app_start) const;

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_url_change) override;

  void SendDesktopNotification(
      ::location::nearby::proto::sharing::DesktopNotification event) const;

  void SendDesktopTransferEvent(
      ::location::nearby::proto::sharing::DesktopTransferEventType event) const;

  std::string Dump() const;

 private:
  void OnEnabledPrefChanged();
  void OnFastInitiationNotificationStatePrefChanged();
  void OnDataUsagePrefChanged();
  void OnVisibilityPrefChanged();
  void OnIsReceivingPrefChanged();
  void OnAllowedContactsPrefChanged();
  void OnIsOnboardingCompletePrefChanged();
  void OnCustomSavePathChanged();
  void OnPreferenceChanged(absl::string_view key);

  void NotifyAllObservers(absl::string_view key, Observer::Data value)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // If the Nearby Share parent feature is toggled on then Fast Initiation
  // notifications should be re-enabled unless the user explicitly disabled the
  // notification sub-feature.
  void ProcessFastInitiationNotificationParentPrefChanged(bool enabled)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void StartVisibilityTimer(absl::Duration expiration) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Restore/Save fallback visibility
  void RestoreFallbackVisibility();

  // Make sure thread safe to access Nearby settings
  mutable RecursiveMutex mutex_;
  nearby::Clock* const clock_;
  nearby::DeviceInfo& device_info_;
  nearby::sharing::api::PreferenceManager& preference_manager_;
  NearbyShareLocalDeviceDataManager* const local_device_data_manager_;
  // Used to create analytics events.
  std::unique_ptr<analytics::AnalyticsRecorder> analytics_recorder_;

  std::shared_ptr<bool> is_desctructing_ = nullptr;
  bool is_fast_initiation_hardware_supported_ ABSL_GUARDED_BY(mutex_) = false;
  ObserverList<Observer> observers_set_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<Timer> visibility_expiration_timer_ ABSL_GUARDED_BY(mutex_);
  mutable std::optional<proto::DeviceVisibility> fallback_visibility_
      ABSL_GUARDED_BY(mutex_);

  // Used to track the timestamp of visibility change.
  mutable absl::Time last_visibility_timestamp_ ABSL_GUARDED_BY(mutex_) =
      absl::InfinitePast();
  mutable proto::DeviceVisibility last_visibility_ ABSL_GUARDED_BY(mutex_) =
      proto::DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SETTINGS_H_
