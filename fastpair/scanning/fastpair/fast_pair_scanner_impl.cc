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
#include <utility>

#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::Duration kFastPairLowPowerActiveSeconds = absl::Seconds(2);
constexpr absl::Duration kFastPairLowPowerInactiveSeconds = absl::Seconds(3);
constexpr char kFastPairServiceUuid[] = "0000FE2C-0000-1000-8000-00805F9B34FB";

class ScanningSessionImpl : public FastPairScanner::ScanningSession {
 public:
  explicit ScanningSessionImpl(FastPairScannerImpl* scanner)
      : scanner_(scanner) {}
  ~ScanningSessionImpl() override {
    NEARBY_LOGS(VERBOSE) << __func__;
    scanner_->StopScanning();
  }

 private:
  FastPairScannerImpl* scanner_;
};

}  // namespace

// FastPairScannerImpl
FastPairScannerImpl::FastPairScannerImpl(Mediums& mediums,
                                         SingleThreadExecutor* executor)
    : mediums_(mediums), executor_(executor) {}

void FastPairScannerImpl::AddObserver(FastPairScanner::Observer* observer) {
  observer_.AddObserver(observer);
}

void FastPairScannerImpl::RemoveObserver(FastPairScanner::Observer* observer) {
  observer_.RemoveObserver(observer);
}

std::unique_ptr<FastPairScanner::ScanningSession>
FastPairScannerImpl::StartScanning() {
  NEARBY_LOGS(VERBOSE) << __func__;
  executor_->Execute("scanning", [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(
                                     *executor_) { StartScanningInternal(); });
  return std::make_unique<ScanningSessionImpl>(this);
}

void FastPairScannerImpl::StartScanningInternal() {
  NEARBY_LOGS(VERBOSE) << __func__;
  if (mediums_.GetBluetoothRadio().Enable() &&
      mediums_.GetBle().IsAvailable() &&
      mediums_.GetBle().StartScanning(
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
      StartTimer(kFastPairLowPowerActiveSeconds,
                 [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                   PauseScanning();
                 });
    }
  } else {
    NEARBY_LOGS(INFO) << "Couldn't start scanning on BLE for service_id = "
                      << kServiceId;
  }
}

void FastPairScannerImpl::StopScanning() {
  NEARBY_LOGS(VERBOSE) << __func__;
  executor_->Execute("stop-scan",
                     [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                       NEARBY_LOGS(VERBOSE) << __func__ << " in background";
                       timer_.reset();
                       mediums_.GetBle().StopScanning(kServiceId);
                     });
}

void FastPairScannerImpl::PauseScanning() {
  DCHECK(IsFastPairLowPowerEnabled());
  mediums_.GetBle().StopScanning(kServiceId);
  StartTimer(kFastPairLowPowerInactiveSeconds,
             [this]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
               StartScanningInternal();
             });
}

void FastPairScannerImpl::StartTimer(absl::Duration delay,
                                     absl::AnyInvocable<void()> callback) {
  timer_ = std::make_unique<TimerImpl>();
  timer_->Start(delay / absl::Milliseconds(1), 0,
                [this, callback = std::move(callback)]()
                    ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
                      executor_->Execute(std::move(callback));
                    });
}

void FastPairScannerImpl::OnDeviceFound(const BlePeripheral& peripheral) {
  NEARBY_LOGS(INFO) << __func__ << "Found device with ble Address = "
                    << peripheral.GetName();
  std::string service_data =
      peripheral.GetAdvertisementBytes(kServiceId).string_data();
  if (service_data.empty()) {
    NEARBY_LOGS(WARNING) << "No Fast Pair service data found on device";
    return;
  }

  device_address_advertisement_data_map_[peripheral.GetName()].insert(
      service_data);

  NotifyDeviceFound(peripheral);
}

void FastPairScannerImpl::OnDeviceLost(const BlePeripheral& peripheral) {
  NEARBY_LOGS(INFO) << __func__ << "Lost device with ble Address = "
                    << peripheral.GetName();
  device_address_advertisement_data_map_.erase(peripheral.GetName());

  for (auto& observer : observer_.GetObservers()) {
    observer->OnDeviceLost(peripheral);
  }
}

void FastPairScannerImpl::NotifyDeviceFound(const BlePeripheral& peripheral) {
  for (auto& observer : observer_.GetObservers()) {
    observer->OnDeviceFound(peripheral);
  }
}

}  // namespace fastpair
}  // namespace nearby
