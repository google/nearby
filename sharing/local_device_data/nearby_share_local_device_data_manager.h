// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_

#include <stddef.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// The maximum length in bytes allowed for a device name, as encoded in UTF-8 in
// a std::string, which will not contain a null terminator.
extern const size_t kNearbyShareDeviceNameMaxLength;

// Handles uploading contacts and certificates to the Nearby server.
class NearbyShareLocalDeviceDataManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnLocalDeviceDataChanged(bool did_device_name_change,
                                          bool did_full_name_change,
                                          bool did_icon_change) = 0;
  };

  NearbyShareLocalDeviceDataManager();
  virtual ~NearbyShareLocalDeviceDataManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the name of the local device, for example, "Josh's Chromebook."
  // This can be modified by SetDeviceName().
  virtual std::string GetDeviceName() const = 0;

  // Sets and persists the device name in prefs. The device name is first
  // validated and if validation fails and error is returned and the device name
  // is not persisted. The device name is *not* uploaded to the Nearby Share
  // server; the UpdateDevice proto device_name field in an artifact. Observers
  // are notified via OnLocalDeviceDataChanged() if the device name changes.
  virtual DeviceNameValidationResult SetDeviceName(absl::string_view name) = 0;

 protected:
  void NotifyLocalDeviceDataChanged(bool did_device_name_change,
                                    bool did_full_name_change,
                                    bool did_icon_change);

 private:
  nearby::ObserverList<Observer> observers_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
