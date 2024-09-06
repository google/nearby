// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_WIFI_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_WIFI_ADAPTER_H_

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace sharing {
namespace api {

class WifiAdapter {
 public:
  enum class PermissionStatus {
    kUndetermined = 0,
    kSystemDenied,
    kUserDenied,
    kAllowed
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the presence of the adapter `adapter` changes. When `present`
    // is true the adapter is now present, false means the adapter has been
    // removed from the system.
    virtual void AdapterPresentChanged(WifiAdapter* adapter, bool present) {}

    // Called when the radio power state of the adapter `adapter` changes. When
    // `powered` is true the adapter radio is powered, false means the adapter
    // radio is off.
    virtual void AdapterPoweredChanged(WifiAdapter* adapter, bool powered) {}
  };

  virtual ~WifiAdapter() = default;

  // Indicates whether the adapter is actually present/not disabled by the
  // device firmware or hardware switch on the system.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Returns the status of the browser's Wi-Fi permission status.
  virtual PermissionStatus GetOsPermissionStatus() const = 0;

  // Requests a change to the adapter radio power. Setting `powered` to true
  // will turn on the radio and false will turn it off. On success,
  // `success_callback` will be called. On failure, `error_callback` will be
  // called.
  virtual void SetPowered(bool powered, std::function<void()> success_callback,
                          std::function<void()> error_callback) = 0;

  // The unique ID/name of this adapter.
  virtual std::optional<std::string> GetAdapterId() const = 0;

  // Adds and removes observers for events on this Wi-Fi adapter. If
  // monitoring multiple adapters, check the `adapter` parameter of observer
  // methods to determine which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;
};

}  // namespace api
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_WIFI_ADAPTER_H_
