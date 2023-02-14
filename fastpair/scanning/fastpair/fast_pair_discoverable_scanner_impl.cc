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

#include "fastpair/scanning/fastpair/fast_pair_discoverable_scanner_impl.h"

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
#include "fastpair/server_access/fast_pair_repository.h"
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
FastPairDiscoverableScannerImpl::Factory*
    FastPairDiscoverableScannerImpl::Factory::g_test_factory_ = nullptr;

std::unique_ptr<FastPairDiscoverableScanner>
FastPairDiscoverableScannerImpl::Factory::Create(
    std::shared_ptr<FastPairScanner> scanner,
    std::shared_ptr<BluetoothAdapter> adapter, DeviceCallback found_callback,
    DeviceCallback lost_callback) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(
        std::move(scanner), std::move(adapter), std::move(found_callback),
        std::move(lost_callback));
  }

  return std::make_unique<FastPairDiscoverableScannerImpl>(
      std::move(scanner), std::move(adapter), std::move(found_callback),
      std::move(lost_callback));
}

void FastPairDiscoverableScannerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairDiscoverableScannerImpl::Factory::~Factory() = default;

// FastPairScannerImpl
FastPairDiscoverableScannerImpl::FastPairDiscoverableScannerImpl(
    std::shared_ptr<FastPairScanner> scanner,
    std::shared_ptr<BluetoothAdapter> adapter, DeviceCallback found_callback,
    DeviceCallback lost_callback)
    : scanner_(std::move(scanner)),
      adapter_(std::move(adapter)),
      found_callback_(std::move(found_callback)),
      lost_callback_(std::move(lost_callback)) {
  scanner_->AddObserver(this);
}

void FastPairDiscoverableScannerImpl::OnDeviceFound(
    const BlePeripheral& peripheral) {
  ByteArray fast_pair_service_data =
      peripheral.GetAdvertisementBytes(kServiceId);
  if (fast_pair_service_data.Empty()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Device doesn't have any Fast Pair Service Data.";
    return;
  }

  model_id_parse_attempts_[peripheral.GetName()] = 1;
  NEARBY_LOGS(INFO) << __func__ << ": Attempting to get model ID";
  std::vector<uint8_t> service_data;
  std::string model_id_bytes = fast_pair_service_data.data();
  std::move(std::begin(model_id_bytes), std::end(model_id_bytes),
            std::back_inserter(service_data));

  FastPairDataParser::GetHexModelIdFromServiceData(
      service_data,
      {[this, peripheral](std::optional<absl::string_view> model_id) {
        OnModelIdRetrieved(peripheral.GetName(), model_id);
      }});
}

void FastPairDiscoverableScannerImpl::OnModelIdRetrieved(
    const std::string& address,
    const std::optional<absl::string_view> model_id) {
  auto it = model_id_parse_attempts_.find(address);

  // If there's no entry in the map, the device was lost while parsing.
  if (it == model_id_parse_attempts_.end()) {
    NEARBY_LOGS(WARNING)
        << __func__
        << ": Returning early because device as lost while parsing.";
    return;
  }

  model_id_parse_attempts_.erase(it);

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
      absl::bind_front(
          &FastPairDiscoverableScannerImpl::OnDeviceMetadataRetrieved, this,
          address, std::string(model_id.value())));
}

void FastPairDiscoverableScannerImpl::OnDeviceMetadataRetrieved(
    const std::string& address, const std::string model_id,
    DeviceMetadata& device_metadata) {
  DCHECK(&device_metadata);
  // Ignore advertisements that aren't for Fast Pair but leverage the service
  // UUID.
  if (!IsValidDeviceType(device_metadata.GetDetails())) {
    NEARBY_LOGS(WARNING)
        << __func__
        << ": Invalid device type for Fast Pair. Ignoring this advertisement";
    return;
  }

  // Ignore advertisements for unsupported notification types, such as
  // APP_LAUNCH which should launch a companion app instead of beginning Fast
  // Pair.
  if (!IsSupportedNotificationType(device_metadata.GetDetails())) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Unsupported notification type for Fast Pair. "
                            "Ignoring this advertisement";
    return;
  }

  FastPairDevice device(model_id, address, Protocol::kFastPairInitialPairing);
  NotifyDeviceFound(device);
}

void FastPairDiscoverableScannerImpl::NotifyDeviceFound(
    FastPairDevice& device) {
  NEARBY_LOGS(VERBOSE) << "Notify Device found:"
                       << "BluetoothAddress = " << device.ble_address
                       << ", Model id = " << device.model_id;
  notified_devices_[device.ble_address] = &device;
  found_callback_(device);
}

void FastPairDiscoverableScannerImpl::OnDeviceLost(
    const BlePeripheral& peripheral) {
  NEARBY_LOGS(INFO) << __func__ << ": Running lost callback";

  model_id_parse_attempts_.erase(peripheral.GetName());

  auto it = notified_devices_.find(peripheral.GetName());

  // Don't invoke callback if we didn't notify this device.
  if (it == notified_devices_.end()) return;
  FastPairDevice* notified_device = it->second;
  notified_devices_.erase(it);
  lost_callback_(*notified_device);
}

}  // namespace fastpair
}  // namespace nearby
