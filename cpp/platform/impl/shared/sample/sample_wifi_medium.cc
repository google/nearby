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

#include "platform/impl/shared/sample/sample_wifi_medium.h"

#include <cstdint>

#include "platform/prng.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace sample {

namespace {

const char* kOpenSSID = "__OPEN__";
const char* kWpaPskSSID = "__WPA_PSK__";
const char* kWepSSID = "__WEP__";
const char* kNoInternetConnectivitySSID = "__NO_INTERNET_CONNECTIVITY__";
const char* kConnectionFailureSSID = "__CONNECTION_FAILURE__";
const char* kAuthFailureSSID = "__AUTH_FAILURE__";

std::uint32_t boundedUInt32(std::uint32_t upper_limit) {
  return Prng().nextUInt32() % (upper_limit + 1);
}

void randomSleep(std::uint32_t upper_limit_millis) {
  absl::SleepFor(absl::Milliseconds(boundedUInt32(upper_limit_millis)));
}

}  // namespace

std::vector<SampleWifiScanResult> SampleWifiMedium::canned_scan_results_;

SampleWifiMedium::SampleWifiMedium() : current_ssid_() {
  // One-time initialization of our static canned_scan_results_.
  if (canned_scan_results_.empty()) {
    canned_scan_results_.push_back(
        SampleWifiScanResult(kOpenSSID, 1, 2401, WifiAuthType::OPEN));
    canned_scan_results_.push_back(
        SampleWifiScanResult(kWpaPskSSID, 2, 5002, WifiAuthType::WPA_PSK));
    canned_scan_results_.push_back(
        SampleWifiScanResult(kWepSSID, 3, 2403, WifiAuthType::WEP));
    canned_scan_results_.push_back(SampleWifiScanResult(
        kNoInternetConnectivitySSID, 4, 5004, WifiAuthType::OPEN));
    canned_scan_results_.push_back(SampleWifiScanResult(
        kConnectionFailureSSID, 5, 2405, WifiAuthType::OPEN));
    canned_scan_results_.push_back(
        SampleWifiScanResult(kAuthFailureSSID, 6, 5006, WifiAuthType::OPEN));
  }
}

SampleWifiMedium::~SampleWifiMedium() {}

bool SampleWifiMedium::scan(
    Ptr<WifiMedium::ScanResultCallback> scan_result_callback) {
  // Sleep for up to 10 seconds, to simulate performing an actual Wifi scan.
  randomSleep(10 * 1000);

  // Construct the response.
  std::vector<ConstPtr<WifiScanResult> > scan_results;
  for (std::vector<SampleWifiScanResult>::const_iterator it =
           canned_scan_results_.begin();
       it != canned_scan_results_.end(); it++) {
    scan_results.push_back(ConstPtr<WifiScanResult>(
        new SampleWifiScanResult(it->getSSID(), it->getSignalStrengthDbm(),
                                 it->getFrequencyMhz(), it->getAuthType())));
  }

  // And report it back.
  scan_result_callback->onScanResults(scan_results);

  return false;
}

WifiConnectionStatus::Value SampleWifiMedium::connectToNetwork(
    const std::string& ssid, const std::string& password,
    WifiAuthType::Value auth_type) {
  // Sleep for up to 10 seconds, to simulate actually connecting to the SSID.
  randomSleep(10 * 1000);

  if (kConnectionFailureSSID == ssid) {
    return WifiConnectionStatus::CONNECTION_FAILURE;
  }

  if (kAuthFailureSSID == ssid) {
    return WifiConnectionStatus::AUTH_FAILURE;
  }

  return WifiConnectionStatus::CONNECTED;
}

bool SampleWifiMedium::verifyInternetConnectivity() {
  if (current_ssid_.empty()) {
    return false;
  }

  // Sleep for up to 5 seconds, to simulate actually verifying internet
  // connectivity.
  randomSleep(5 * 1000);

  return current_ssid_ != kNoInternetConnectivitySSID;
}

std::string SampleWifiMedium::getIPAddress() {
  return current_ssid_.empty() ? "" : "1.2.3.4";
}

}  // namespace sample
}  // namespace nearby
}  // namespace location
