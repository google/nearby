// Copyright 2020 Google LLC
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

#ifndef PLATFORM_API_WIFI_H_
#define PLATFORM_API_WIFI_H_

#include <cstdint>
#include <vector>

#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Possible authentication types for a WiFi network.
struct WifiAuthType {
  enum Value {
    UNKNOWN = 0,
    OPEN = 1,
    WPA_PSK = 2,
    WEP = 3,
  };
};

// Possible statuses of a device's connection to a WiFi network.
struct WifiConnectionStatus {
  enum Value {
    UNKNOWN = 0,
    CONNECTED = 1,
    CONNECTION_FAILURE = 2,
    AUTH_FAILURE = 3,
  };
};

// Represents a WiFi network found during a call to WifiMedium#scan().
class WifiScanResult {
 public:
  virtual ~WifiScanResult() {}

  // Gets the SSID of this WiFi network.
  virtual std::string getSSID() const = 0;
  // Gets the signal strength of this WiFi network in dBm.
  virtual std::int32_t getSignalStrengthDbm() const = 0;
  // Gets the frequency band of this WiFi network in MHz.
  virtual std::int32_t getFrequencyMhz() const = 0;
  // Gets the authentication type of this WiFi network.
  virtual WifiAuthType::Value getAuthType() const = 0;
};

// Container of operations that can be performed over the WiFi medium.
class WifiMedium {
 public:
  virtual ~WifiMedium() {}

  class ScanResultCallback {
   public:
    virtual ~ScanResultCallback() {}

    // The ConstPtr<WifiScanResult> objects contained in scan_results will be
    // owned (and destroyed) by the recipient of the callback methods (i.e. the
    // creator of the concrete ScanResultCallback object).
    virtual void onScanResults(
        const std::vector<ConstPtr<WifiScanResult>>& scan_results) = 0;
  };

  // Does not take ownership of the passed-in scan_result_callback -- destroying
  // that is up to the caller.
  virtual bool scan(Ptr<ScanResultCallback> scan_result_callback) = 0;

  // If 'password' is an empty string, none has been provided. Returns
  // WifiConnectionStatus::CONNECTED on success, or the appropriate failure code
  // otherwise.
  virtual WifiConnectionStatus::Value connectToNetwork(
      const std::string& ssid,
      const std::string& password,
      WifiAuthType::Value auth_type) = 0;

  // Blocks until it's certain of there being a connection to the internet, or
  // returns false if it fails to do so.
  //
  // How this method wants to verify said connection is totally up to it (so it
  // can feel free to ping whatever server, download whatever resource, etc.
  // that it needs to gain confidence that the internet is reachable hereon in).
  virtual bool verifyInternetConnectivity() = 0;

  // Returns the local device's IP address in the IPv4 dotted-quad format.
  virtual std::string getIPAddress() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WIFI_H_
