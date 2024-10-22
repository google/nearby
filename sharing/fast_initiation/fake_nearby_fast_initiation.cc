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

#include "sharing/fast_initiation/fake_nearby_fast_initiation.h"

#include <functional>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "sharing/fast_initiation/nearby_fast_initiation.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"

namespace nearby {
namespace sharing {

void FakeNearbyFastInitiation::Factory::SetStartScanningError(
    bool is_start_scanning_error) {
  is_start_scanning_error_ = is_start_scanning_error;
}

void FakeNearbyFastInitiation::Factory::SetStartAdvertisingError(
    bool is_start_advertising_error) {
  is_start_advertising_error_ = is_start_advertising_error;
}

void FakeNearbyFastInitiation::Factory::SetLowEnergySupported(
    bool is_low_energy_supported) {
  is_low_energy_supported_ = is_low_energy_supported;
}

void FakeNearbyFastInitiation::Factory::SetScanOffloadSupported(
    bool is_scan_offload_supported) {
  is_scan_offload_supported_ = is_scan_offload_supported;
}

void FakeNearbyFastInitiation::Factory::SetAdvertisementOffloadSupported(
    bool is_advertisement_offload_supported) {
  is_advertisement_offload_supported_ = is_advertisement_offload_supported;
}

FakeNearbyFastInitiation*
FakeNearbyFastInitiation::Factory::GetNearbyFastInitiation() {
  return fake_nearby_fast_initiation_;
}

std::unique_ptr<NearbyFastInitiation>
FakeNearbyFastInitiation::Factory::CreateInstance(Context* context) {
  fake_nearby_fast_initiation_ = new FakeNearbyFastInitiation(context);
  fake_nearby_fast_initiation_->SetStartScanningError(is_start_scanning_error_);
  fake_nearby_fast_initiation_->SetStartAdvertisingError(
      is_start_advertising_error_);
  fake_nearby_fast_initiation_->SetLowEnergySupported(is_low_energy_supported_);
  fake_nearby_fast_initiation_->SetScanOffloadSupported(
      is_scan_offload_supported_);
  fake_nearby_fast_initiation_->SetAdvertisementOffloadSupported(
      is_advertisement_offload_supported_);
  return absl::WrapUnique<NearbyFastInitiation>(fake_nearby_fast_initiation_);
}

FakeNearbyFastInitiation::FakeNearbyFastInitiation(Context* context)
    : context_(context) {
  NL_DCHECK(context_);
}

bool FakeNearbyFastInitiation::IsLowEnergySupported() {
  return is_low_energy_supported_;
}

bool FakeNearbyFastInitiation::IsScanOffloadSupported() {
  return is_scan_offload_supported_;
}

bool FakeNearbyFastInitiation::IsAdvertisementOffloadSupported() {
  return is_advertisement_offload_supported_;
}

void FakeNearbyFastInitiation::StartScanning(
    std::function<void()> devices_discovered_callback,
    std::function<void()> devices_not_discovered_callback,
    std::function<void()> error_callback) {
  ++start_scanning_call_count_;
  if (is_start_scanning_error_) {
    error_callback();
    return;
  }

  is_scanning_ = true;
  scanning_devices_discovered_callback_ =
      std::move(devices_discovered_callback);
  scanning_devices_not_discovered_callback_ =
      std::move(devices_not_discovered_callback);
}

void FakeNearbyFastInitiation::StopScanning(std::function<void()> callback) {
  ++stop_scanning_call_count_;
  is_scanning_ = false;
  callback();
}

void FakeNearbyFastInitiation::StartAdvertising(
    FastInitType type, std::function<void()> callback,
    std::function<void()> error_callback) {
  ++start_advertising_call_count_;
  if (is_start_advertising_error_) {
    error_callback();
  } else {
    is_advertising_ = true;
    callback();
  }
}

void FakeNearbyFastInitiation::StopAdvertising(std::function<void()> callback) {
  ++stop_advertising_call_count_;
  is_advertising_ = false;
  callback();
}

void FakeNearbyFastInitiation::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}
void FakeNearbyFastInitiation::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
bool FakeNearbyFastInitiation::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

// Fake methods
void FakeNearbyFastInitiation::SetStartScanningError(
    bool is_start_scanning_error) {
  is_start_scanning_error_ = is_start_scanning_error;
  for (Observer* observer : observer_list_.GetObservers()) {
    if (observer != nullptr) {
      observer->HardwareErrorReported(this);
    }
  }
}

void FakeNearbyFastInitiation::SetStartAdvertisingError(
    bool is_start_advertising_error) {
  is_start_advertising_error_ = is_start_advertising_error;
  for (Observer* observer : observer_list_.GetObservers()) {
    if (observer != nullptr) {
      observer->HardwareErrorReported(this);
    }
  }
}

void FakeNearbyFastInitiation::SetLowEnergySupported(
    bool is_low_energy_supported) {
  is_low_energy_supported_ = is_low_energy_supported;
}

void FakeNearbyFastInitiation::SetScanOffloadSupported(
    bool is_scan_offload_supported) {
  is_scan_offload_supported_ = is_scan_offload_supported;
}

void FakeNearbyFastInitiation::SetAdvertisementOffloadSupported(
    bool is_advertisement_offload_supported) {
  is_advertisement_offload_supported_ = is_advertisement_offload_supported;
}

int FakeNearbyFastInitiation::StartScanningCount() const {
  return start_scanning_call_count_;
}

int FakeNearbyFastInitiation::StopScanningCount() const {
  return stop_scanning_call_count_;
}

int FakeNearbyFastInitiation::StartAdvertisingCount() const {
  return start_advertising_call_count_;
}

int FakeNearbyFastInitiation::StopAdvertisingCount() const {
  return stop_advertising_call_count_;
}

void FakeNearbyFastInitiation::FireDevicesDetected() {
  scanning_devices_discovered_callback_();
}

void FakeNearbyFastInitiation::FireDevicesNotDetected() {
  scanning_devices_not_discovered_callback_();
}

}  // namespace sharing
}  // namespace nearby
