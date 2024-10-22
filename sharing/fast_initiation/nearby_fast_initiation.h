// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_NEARBY_FAST_INITIATION_H_
#define THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_NEARBY_FAST_INITIATION_H_

#include <stdint.h>

#include <functional>

namespace nearby {
namespace sharing {

class NearbyFastInitiation {
 public:
  enum class Version { kV1 = 0 };

  // Fast initialization type impacts on the metadata information in payload of
  // advertising.
  // kNotify = The sender is in the foreground, actively trying to send a file
  // to another device. Receivers in the room should be alerted that they may
  // need to enter Everyone mode to receive the file.
  // kSilent = The phone is passively looking for nearby receivers, but the user
  // has not explicitly tried to enter Nearby Share to send the file. The phone
  // may cache the receivers nearby so that discovery is faster, or it may
  // present these users on ambient surfaces
  enum class FastInitType : uint8_t {
    kNotify = 0,
    kSilent = 1,
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when hardware error reported that requires PC restart
    virtual void HardwareErrorReported(NearbyFastInitiation* fast_init) {}
  };

  virtual ~NearbyFastInitiation() = default;

  virtual bool IsLowEnergySupported() = 0;
  virtual bool IsScanOffloadSupported() = 0;
  virtual bool IsAdvertisementOffloadSupported() = 0;

  // Scans nearby devices using BLE.
  // |devices_discovered_callback| is called when discovered devices from
  // 0 to more. |devices_not_discovered_callback| is called when discovered
  // devices become to 0 from more than one. |error_callback| is called
  // when error happened.
  virtual void StartScanning(
      std::function<void()> devices_discovered_callback,
      std::function<void()> devices_not_discovered_callback,
      std::function<void()> error_callback) = 0;

  // Stops Fast Initialization scanning. |callback| is called
  // when scanning is stopped.
  virtual void StopScanning(std::function<void()> callback) = 0;

  // Begins broadcasting Fast Initiation advertisement. |callback| is called
  // when advertising is started. |error_callback| is called if start
  // advertising with errors.
  // Note: FastInitType is currently hardcoded to kNotify under the hood. To
  // support changing the FastInitType after initialization, additional APIs
  // need to be added/refactored to Context to reinitialize/reset the
  // FastInitiationManager with the desired type.
  virtual void StartAdvertising(FastInitType type,
                                std::function<void()> callback,
                                std::function<void()> error_callback) = 0;

  // Stop broadcasting Fast Initiation advertisements. |callback|
  // is called when advertising is stopped.
  virtual void StopAdvertising(std::function<void()> callback) = 0;

  // Check the scanning status.
  virtual bool IsScanning() const = 0;

  // Check the advertising status.
  virtual bool IsAdvertising() const = 0;

  // Adds and removes observers for hardware error events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAST_INITIATION_NEARBY_FAST_INITIATION_H_
