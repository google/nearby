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

#include "fastpair/scanning/fastpair/fast_pair_scanner_impl.h"

#include <memory>
#include <string>

#include "fastpair/common/constant.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"
#include "internal/platform/task_runner_impl.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::Duration kFastPairLowPowerActiveSeconds = absl::Seconds(2);
constexpr absl::Duration kFastPairLowPowerInactiveSeconds = absl::Seconds(3);
}  // namespace

// static
FastPairScannerImpl::Factory* FastPairScannerImpl::Factory::g_test_factory_ =
    nullptr;

// static
std::shared_ptr<FastPairScanner> FastPairScannerImpl::Factory::Create() {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance();
  }

  return std::make_shared<FastPairScannerImpl>();
}

// static
void FastPairScannerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairScannerImpl::Factory::~Factory() = default;

// FastPairScannerImpl
FastPairScannerImpl::FastPairScannerImpl() {
  task_runner_ = std::make_shared<TaskRunnerImpl>(1);
}

void FastPairScannerImpl::AddObserver(FastPairScanner::Observer* observer) {
  observer_.AddObserver(observer);
}

void FastPairScannerImpl::RemoveObserver(FastPairScanner::Observer* observer) {
  observer_.RemoveObserver(observer);
}

void FastPairScannerImpl::StartScanning() {
  task_runner_->PostTask(
      [this]() {
        if (ble_.Enable() &&
            ble_.StartScanning(
                kServiceId, kFastPairServiceUuid,
                {
                    .peripheral_discovered_cb =
                        [this](BlePeripheral& peripheral,
                               const std::string& service_id,
                               const ByteArray& medium_advertisement_bytes,
                               bool fast_advertisement) {
                          if (service_id != kServiceId) {
                            NEARBY_LOGS(INFO)
                                << "Skipping non-fastpair advertisement";
                            return;
                          }
                          OnDeviceFound(peripheral);
                        },
                    .peripheral_lost_cb =
                        [this](BlePeripheral& peripheral,
                               const std::string& service_id) {
                          OnDeviceLost(peripheral);
                        },
                })) {
          NEARBY_LOGS(INFO)
              << "Started scanning for BLE advertisements for service_id = "
              << kServiceId;
          if (IsFastPairLowPowerEnabled()) {
            task_runner_->PostDelayedTask(kFastPairLowPowerActiveSeconds,
                                          [this]() { StopScanning(); });
          }
        } else {
          NEARBY_LOGS(INFO)
              << "Couldn't start scanning on BLE for service_id = "
              << kServiceId;
        }
      });
}

void FastPairScannerImpl::StopScanning() {
  DCHECK(IsFastPairLowPowerEnabled());
  ble_.StopScanning(kServiceId);
  task_runner_->PostDelayedTask(kFastPairLowPowerInactiveSeconds,
                                [this]() { StartScanning(); });
}

void FastPairScannerImpl::OnDeviceFound(const BlePeripheral& peripheral) {
  NEARBY_LOGS(INFO) << __func__ << "Found device with ble Address = "
                    << peripheral.GetName();
  ByteArray service_data = peripheral.GetAdvertisementBytes(kServiceId);
  if (service_data.Empty()) {
    NEARBY_LOGS(WARNING) << "No Fast Pair service data found on device";
    return;
  }

  device_address_advertisement_data_map_[peripheral.GetName()].insert(
      peripheral.GetAdvertisementBytes(kServiceId));

  NotifyDeviceFound(peripheral);
}

void FastPairScannerImpl::OnDeviceLost(const BlePeripheral& peripheral) {
  NEARBY_LOGS(INFO) << __func__ << "Lost device with ble Address = "
                    << peripheral.GetName();
  device_address_advertisement_data_map_.erase(peripheral.GetName());

  for (auto& observer : observer_) {
    observer->OnDeviceLost(peripheral);
  }
}

void FastPairScannerImpl::NotifyDeviceFound(const BlePeripheral& peripheral) {
  for (auto& observer : observer_) {
    observer->OnDeviceFound(peripheral);
  }
}

}  // namespace fastpair
}  // namespace nearby
