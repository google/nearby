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
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/base/observer_list.h"
#include "internal/platform/clock.h"
#include "internal/platform/device_info.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/settings_observer_data.pb.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {

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
            NL_LOG(FATAL) << "Invalid tag: " << this->tag;
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

  struct FallbackVisibilityInfo {
    proto::DeviceVisibility visibility;
    absl::Time fallback_time;
  };

  NearbyShareSettings(
      Context* context, nearby::Clock* clock, nearby::DeviceInfo& device_info,
      nearby::sharing::api::PreferenceManager& preference_manager,
      NearbyShareLocalDeviceDataManager* local_device_data_manager,
      analytics::AnalyticsRecorder* analytics_recorder = nullptr);
  ~NearbyShareSettings() override;

  // Internal synchronous getters for C++ clients
  proto::FastInitiationNotificationState GetFastInitiationNotificationState()
      const;
  bool is_fast_initiation_hardware_supported() ABSL_LOCKS_EXCLUDED(mutex_);
  void SetIsFastInitiationHardwareSupported(bool is_supported)
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetDeviceName() const;
  proto::DataUsage GetDataUsage() const;
  proto::DeviceVisibility GetVisibility() const;
  // Gets the timestamp of last visibility change. Need the timestamp to decide
  // whether need to send optional signature data during key pairing.
  absl::Time GetLastVisibilityTimestamp() const ABSL_LOCKS_EXCLUDED(mutex_);
  proto::DeviceVisibility GetLastVisibility() const ABSL_LOCKS_EXCLUDED(mutex_);

  std::string GetCustomSavePath() const;

  // Returns true if the feature is disabled by policy.
  bool IsDisabledByPolicy() const;

  // Asynchronous APIs exposed by NearbyShareSettings
  void AddSettingsObserver(Observer* observer);
  void RemoveSettingsObserver(Observer* observer);
  void SetFastInitiationNotificationState(
      proto::FastInitiationNotificationState state);
  void ValidateDeviceName(
      absl::string_view device_name,
      std::function<void(DeviceNameValidationResult)> callback);
  void SetDeviceName(absl::string_view device_name,
                     std::function<void(DeviceNameValidationResult)> callback);
  void SetDataUsage(proto::DataUsage data_usage);
  // Returns the fallback visibility if the current visibility is temporary,
  // otherwise returning |DEVICE_VISIBILITY_UNSPECIFIED|.
  FallbackVisibilityInfo GetFallbackVisibility() const
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Sets the visibility for the Nearby Sharing service. If the expiration is
  // not zero, the visibility will be set temporarily and a fallback will be
  // set. If the expiration is zero, the visibility will be set permanently.
  // Note: When transitioning between temporary and permanent everyone mode
  // visibility, the fallback visibility will not be cleared. However, when the
  // temporary timer expires, the fallback visibility will be restored and
  // cleared.
  void SetVisibility(proto::DeviceVisibility visibility,
                     absl::Duration expiration = absl::ZeroDuration())
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool GetIsAnalyticsEnabled() const;
  void SetIsAnalyticsEnabled(bool is_analytics_enabled);

  void SetCustomSavePathAsync(absl::string_view save_path,
                              const std::function<void()>& callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_url_change) override;

  std::string Dump() const;

 private:
  void SetFallbackVisibility(proto::DeviceVisibility visibility)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void OnPreferenceChanged(absl::string_view key);

  void NotifyAllObservers(absl::string_view key, Observer::Data value);

  void StartVisibilityTimer(absl::Duration expiration)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  FallbackVisibilityInfo GetRawFallbackVisibility() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Restore/Save fallback visibility
  void RestoreFallbackVisibility()
      ABSL_NO_THREAD_SAFETY_ANALYSIS;  // called from c'tor only

  // Make sure thread safe to access Nearby settings
  mutable absl::Mutex mutex_;
  Context* context_;
  nearby::Clock* const clock_;
  nearby::DeviceInfo& device_info_;
  nearby::sharing::api::PreferenceManager& preference_manager_;
  NearbyShareLocalDeviceDataManager* const local_device_data_manager_;
  // Used to create analytics events.
  analytics::AnalyticsRecorder* const analytics_recorder_;

  std::shared_ptr<bool> is_desctructing_ = nullptr;
  bool is_fast_initiation_hardware_supported_ ABSL_GUARDED_BY(mutex_) = false;
  ObserverList<Observer> observers_set_;
  std::unique_ptr<ThreadTimer> visibility_expiration_timer_
      ABSL_GUARDED_BY(mutex_);
  proto::DeviceVisibility fallback_visibility_ ABSL_GUARDED_BY(mutex_);

  // Used to track the timestamp of visibility change.
  absl::Time last_visibility_timestamp_ ABSL_GUARDED_BY(mutex_) =
      absl::InfinitePast();
  proto::DeviceVisibility last_visibility_ ABSL_GUARDED_BY(mutex_) =
      proto::DeviceVisibility::DEVICE_VISIBILITY_UNSPECIFIED;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SETTINGS_H_
