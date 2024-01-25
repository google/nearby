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

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {

// The maximum length in bytes allowed for a device name, as encoded in UTF-8 in
// a std::string, which will not contain a null terminator.
extern const size_t kNearbyShareDeviceNameMaxLength;

// Manages local device data related to the UpdateDevice RPC such as the device
// ID, name, and icon URL; provides the user's full name and icon URL returned
// from the Nearby server; and handles uploading contacts and certificates to
// the Nearby server. The uploading of contacts and certificates might seem out
// of place, but this class is the entry point for  all UpdateDevice RPC calls.
class NearbyShareLocalDeviceDataManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnLocalDeviceDataChanged(bool did_device_name_change,
                                          bool did_full_name_change,
                                          bool did_icon_change) = 0;
  };

  using UploadCompleteCallback = std::function<void(bool success)>;

  NearbyShareLocalDeviceDataManager();
  virtual ~NearbyShareLocalDeviceDataManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops local-device-data task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  // Returns the immutable ID generated for the local device, used to
  // differentiate a user's devices when communicating with the Nearby server.
  virtual std::string GetId() = 0;

  // Returns the name of the local device, for example, "Josh's Chromebook."
  // This can be modified by SetDeviceName().
  virtual std::string GetDeviceName() const = 0;

  // Returns the user's full name, for example, "Barack Obama". Returns
  // absl::nullopt if the name has not yet been set from an UpdateDevice RPC
  // response.
  virtual std::optional<std::string> GetFullName() const = 0;

  // Returns the URL of the user's image. Returns absl::nullopt if the URL has
  // not yet been set from an UpdateDevice RPC response.
  virtual std::optional<std::string> GetIconUrl() const = 0;

  // Validates the provided device name and returns an error if validation
  // fails. This is just a check and the device name is not persisted.
  virtual DeviceNameValidationResult ValidateDeviceName(
      absl::string_view name) = 0;

  // Sets and persists the device name in prefs. The device name is first
  // validated and if validation fails and error is returned and the device name
  // is not persisted. The device name is *not* uploaded to the Nearby Share
  // server; the UpdateDevice proto device_name field in an artifact. Observers
  // are notified via OnLocalDeviceDataChanged() if the device name changes.
  virtual DeviceNameValidationResult SetDeviceName(absl::string_view name) = 0;

  // Makes an UpdateDevice RPC call to the Nearby Share server to retrieve all
  // available device data, which includes the full name and icon URL for now.
  // This action is also scheduled periodically. Observers are notified via
  // OnLocalDeviceDataChanged() if any device data changes.
  virtual void DownloadDeviceData() = 0;

  // Uses the UpdateDevice RPC to send the local device's contact list to the
  // Nearby Share server, including which contacts are allowed for
  // selected-contacts visibility mode. This should only be invoked by the
  // contact manager, and the contact manager should handle scheduling, failure
  // retry, etc.
  virtual void UploadContacts(
      std::vector<nearby::sharing::proto::Contact> contacts,
      UploadCompleteCallback callback) = 0;

  // Uses the UpdateDevice RPC to send the local device's public certificates to
  // the Nearby Share server. This should only be invoked by the certificate
  // manager, and the certificate manager should handle scheduling, failure
  // retry, etc.
  virtual void UploadCertificates(
      std::vector<nearby::sharing::proto::PublicCertificate> certificates,
      UploadCompleteCallback callback) = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NotifyLocalDeviceDataChanged(bool did_device_name_change,
                                    bool did_full_name_change,
                                    bool did_icon_change);

 private:
  bool is_running_ = false;
  nearby::ObserverList<Observer> observers_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
