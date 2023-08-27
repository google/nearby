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

#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/protocol.h"
#include "fastpair/dataparser/fast_pair_data_parser.h"
#include "fastpair/proto/fastpair_rpcs.pb.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr char kNearbyShareModelId[] = "fc128e";

bool IsValidDeviceType(const proto::Device& device) {
  return device.device_type() == proto::DeviceType::HEADPHONES ||
         device.device_type() == proto::DeviceType::SPEAKER ||
         device.device_type() == proto::DeviceType::TRUE_WIRELESS_HEADPHONES ||
         device.device_type() == proto::DeviceType::DEVICE_TYPE_UNSPECIFIED;
}

bool IsSupportedNotificationType(const proto::Device& device) {
  // We only allow-list notification types that should trigger a pairing
  // notification, since we currently only support pairing. We include
  // NOTIFICATION_TYPE_UNSPECIFIED to handle the case where a Provider is
  // advertising incorrectly and conservatively allow it to show a notification,
  // matching Android behavior.
  return device.notification_type() ==
             proto::NotificationType::NOTIFICATION_TYPE_UNSPECIFIED ||
         device.notification_type() == proto::NotificationType::FAST_PAIR ||
         device.notification_type() == proto::NotificationType::FAST_PAIR_ONE;
}
}  // namespace

// FastPairScannerImpl::Factory
FastPairDiscoverableScanner::Factory*
    FastPairDiscoverableScanner::Factory::g_test_factory_ = nullptr;

std::unique_ptr<FastPairDiscoverableScanner>
FastPairDiscoverableScanner::Factory::Create(
    FastPairScanner& scanner, DiscoverableScannerCallback found_callback,
    DiscoverableScannerCallback lost_callback, SingleThreadExecutor* executor,
    FastPairDeviceRepository* device_repository) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(scanner, std::move(found_callback),
                                           std::move(lost_callback), executor,
                                           device_repository);
  }

  return std::make_unique<FastPairDiscoverableScanner>(
      scanner, std::move(found_callback), std::move(lost_callback), executor,
      device_repository);
}

void FastPairDiscoverableScanner::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairDiscoverableScanner::Factory::~Factory() = default;

// FastPairScannerImpl
FastPairDiscoverableScanner::FastPairDiscoverableScanner(
    FastPairScanner& scanner, DiscoverableScannerCallback found_callback,
    DiscoverableScannerCallback lost_callback, SingleThreadExecutor* executor,
    FastPairDeviceRepository* device_repository)
    : scanner_(scanner),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)),
      executor_(executor),
      device_repository_(device_repository) {
  scanner_.AddObserver(this);
}

FastPairDiscoverableScanner::~FastPairDiscoverableScanner() {
  scanner_.RemoveObserver(this);
}

void FastPairDiscoverableScanner::OnDeviceFound(
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
       address = peripheral.GetName()]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
            NEARBY_LOGS(INFO) << __func__ << ": Attempting to get model ID";
            std::vector<uint8_t> service_data;
            std::move(std::begin(fast_pair_service_data),
                      std::end(fast_pair_service_data),
                      std::back_inserter(service_data));

            // TODO(jsobczak): GetHexModelIdFromServiceData() callback is
            // synchronous. It would be simpler if
            // GetHexModelIdFromServiceData() used return value rather than a
            // callback.
            FastPairDataParser::GetHexModelIdFromServiceData(
                service_data, {[&](std::optional<absl::string_view> model_id) {
                  OnModelIdRetrieved(address, model_id);
                }});
          });
}

void FastPairDiscoverableScanner::OnModelIdRetrieved(
    const std::string& address,
    const std::optional<absl::string_view> model_id) {
  if (!model_id.has_value()) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Returning early because no model id was parsed.";
    return;
  }

  // The Nearby Share feature advertises under the Fast Pair Service Data UUID
  // and uses a reserved model ID to enable their 'fast initiation' scenario.
  // We must detect this instance and ignore these advertisements since they
  // do not correspond to Fast Pair devices that are open to pairing.
  if (model_id.value().compare(kNearbyShareModelId) == 0) {
    NEARBY_LOGS(WARNING) << __func__ << ": Ignore Nearby Share Model ID.";
    return;
  }

  NEARBY_LOGS(INFO) << __func__ << ": Attempting to get device metadata.";
  FastPairRepository::Get()->GetDeviceMetadata(
      model_id.value(),
      absl::bind_front(&FastPairDiscoverableScanner::OnDeviceMetadataRetrieved,
                       this, address, std::string(model_id.value())));
}

void FastPairDiscoverableScanner::OnDeviceMetadataRetrieved(
    const std::string address, const std::string model_id,
    std::optional<DeviceMetadata> device_metadata) {
  if (!device_metadata.has_value()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Failed to get device metadata";
    return;
  }
  // Ignore advertisements that aren't for Fast Pair but leverage the service
  // UUID.
  if (!IsValidDeviceType(device_metadata->GetDetails())) {
    NEARBY_LOGS(WARNING)
        << __func__
        << ": Invalid device type for Fast Pair. Ignoring this advertisement";
    return;
  }

  // Ignore advertisements for unsupported notification types, such as
  // APP_LAUNCH which should launch a companion app instead of beginning Fast
  // Pair.
  if (!IsSupportedNotificationType(device_metadata->GetDetails())) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Unsupported notification type for Fast Pair. "
                            "Ignoring this advertisement";
    return;
  }
  auto fast_pair_device = std::make_unique<FastPairDevice>(
      model_id, address, Protocol::kFastPairInitialPairing);
  fast_pair_device->SetMetadata(device_metadata.value());

  executor_->Execute(
      "add-device",
      [this, fast_pair_device = std::move(fast_pair_device)]()
          ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
            FastPairDevice* device =
                device_repository_->AddDevice(std::move(fast_pair_device));
            NotifyDeviceFound(*device);
          });
}

void FastPairDiscoverableScanner::NotifyDeviceFound(FastPairDevice& device) {
  NEARBY_LOGS(VERBOSE) << "Notify Device found:"
                       << "BluetoothAddress = " << device.GetBleAddress()
                       << ", Model id = " << device.GetModelId();
  {
    MutexLock lock(&mutex_);
    notified_devices_[device.GetBleAddress()] = &device;
  }
  found_callback_(device);
}

void FastPairDiscoverableScanner::OnDeviceLost(
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
