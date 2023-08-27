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

#include "fastpair/scanning/fastpair/fast_pair_non_discoverable_scanner.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/non_discoverable_advertisement.h"
#include "fastpair/dataparser/fast_pair_data_parser.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

// static
FastPairNonDiscoverableScanner::Factory*
    FastPairNonDiscoverableScanner::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairNonDiscoverableScanner>
FastPairNonDiscoverableScanner::Factory::Create(
    FastPairScanner& scanner, NonDiscoverableScannerCallback found_callback,
    NonDiscoverableScannerCallback lost_callback,
    SingleThreadExecutor* executor,
    FastPairDeviceRepository* device_repository) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(scanner, std::move(found_callback),
                                           std::move(lost_callback), executor,
                                           device_repository);
  }

  return std::make_unique<FastPairNonDiscoverableScanner>(
      scanner, std::move(found_callback), std::move(lost_callback), executor,
      device_repository);
}

// static
void FastPairNonDiscoverableScanner::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairNonDiscoverableScanner::FastPairNonDiscoverableScanner(
    FastPairScanner& scanner, NonDiscoverableScannerCallback found_callback,
    NonDiscoverableScannerCallback lost_callback,
    SingleThreadExecutor* executor, FastPairDeviceRepository* device_repository)
    : scanner_(scanner),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)),
      executor_(executor),
      device_repository_(device_repository) {
  scanner_.AddObserver(this);
}

FastPairNonDiscoverableScanner::~FastPairNonDiscoverableScanner() {
  scanner_.RemoveObserver(this);
}

void FastPairNonDiscoverableScanner::OnDeviceFound(
    const BlePeripheral& peripheral) {
  std::string fast_pair_service_data =
      peripheral.GetAdvertisementBytes(kServiceId).string_data();
  if (fast_pair_service_data.empty()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Device doesn't have any Fast Pair Service Data.";
    return;
  }
  executor_->Execute(
      "device-found",
      [this, fast_pair_service_data = std::move(fast_pair_service_data),
       address =
           peripheral.GetName()]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
        NEARBY_LOGS(INFO) << __func__ << ": Attempting to parse advertisement.";
        FastPairDataParser::ParseNotDiscoverableAdvertisement(
            fast_pair_service_data, address,
            [&](std::optional<NonDiscoverableAdvertisement> advertisement) {
              OnAdvertisementParsed(address, advertisement);
            });
      });
}

void FastPairNonDiscoverableScanner::OnAdvertisementParsed(
    absl::string_view address,
    std::optional<NonDiscoverableAdvertisement> advertisement) {
  if (!advertisement.has_value()) {
    NEARBY_LOGS(INFO)
        << __func__ << ": Returning early because no advertisement was parsed.";
    return;
  }
  NEARBY_LOGS(INFO)
      << __func__
      << ": Attempting to check if device is associated with current account.";
  AccountKeyFilter account_filter(advertisement.value());
  FastPairRepository::Get()->CheckIfAssociatedWithCurrentAccount(
      account_filter, [&, address = std::string(address),
                       advertisement = std::move(advertisement)](
                          std::optional<AccountKey> account_key,
                          std::optional<absl::string_view> model_id) {
        OnAccountKeyFilterCheckResult(address, advertisement, account_key,
                                      model_id);
      });
}

void FastPairNonDiscoverableScanner::OnAccountKeyFilterCheckResult(
    absl::string_view address,
    std::optional<NonDiscoverableAdvertisement> advertisement,
    std::optional<AccountKey> account_key,
    std::optional<absl::string_view> model_id) {
  if (!account_key.has_value() || !model_id.has_value()) {
    return;
  }
  NEARBY_LOGS(INFO) << __func__ << ": Attempting to get device metadata.";
  FastPairRepository::Get()->GetDeviceMetadata(
      model_id.value(),
      [&, address = std::string(address),
       advertisement = std::move(advertisement), model_id = model_id.value(),
       account_key =
           account_key.value()](std::optional<DeviceMetadata> device_metadata) {
        OnDeviceMetadataRetrieved(address, advertisement, model_id, account_key,
                                  device_metadata);
      });
}

void FastPairNonDiscoverableScanner::OnDeviceMetadataRetrieved(
    absl::string_view address,
    std::optional<NonDiscoverableAdvertisement> advertisement,
    absl::string_view model_id, AccountKey account_key,
    std::optional<DeviceMetadata> device_metadata) {
  if (!device_metadata.has_value()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Failed to get device metadata";
    return;
  }

  auto fast_pair_device = std::make_unique<FastPairDevice>(
      model_id, address, Protocol::kFastPairSubsequentPairing);
  fast_pair_device->SetAccountKey(account_key);
  fast_pair_device->SetMetadata(device_metadata.value());
  fast_pair_device->SetShowUiNotification(
      advertisement->type == NonDiscoverableAdvertisement::Type::kShowUi);
  executor_->Execute(
      "add-device",
      [this, fast_pair_device = std::move(fast_pair_device)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
            FastPairDevice* device =
                device_repository_->AddDevice(std::move(fast_pair_device));
            NotifyDeviceFound(*device);
          });
}

void FastPairNonDiscoverableScanner::NotifyDeviceFound(FastPairDevice& device) {
  NEARBY_LOGS(VERBOSE) << "Notify Device found:"
                       << "BluetoothAddress = " << device.GetBleAddress()
                       << ", Model id = " << device.GetModelId();
  {
    MutexLock lock(&mutex_);
    notified_devices_[device.GetBleAddress()] = &device;
  }
  found_callback_(device);
}

void FastPairNonDiscoverableScanner::OnDeviceLost(
    const BlePeripheral& peripheral) {
  NEARBY_LOGS(INFO) << __func__ << ": Running lost callback";
  {
    MutexLock lock(&mutex_);
    auto node = notified_devices_.extract(peripheral.GetName());
    // Don't invoke callback if we didn't notify this device.
    if (node.empty()) return;
  }
  executor_->Execute("device-lost",
                     [this, address = peripheral.GetName()]()
                         ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                           auto opt_device =
                               device_repository_->FindDevice(address);
                           if (!opt_device.has_value()) return;
                           FastPairDevice* device = opt_device.value();
                           lost_callback_(*device);
                           device_repository_->RemoveDevice(device);
                         });
}

}  // namespace fastpair
}  // namespace nearby
