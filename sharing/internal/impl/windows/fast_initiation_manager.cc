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

#include "sharing/internal/impl/windows/fast_initiation_manager.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Storage.Streams.h"

namespace nearby::sharing::windows {
namespace {

using ::winrt::Windows::Devices::Bluetooth::BluetoothError;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisement;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs;
using ::winrt::Windows::Storage::Streams::DataWriter;
using Error = ::nearby::api::FastInitiationManager::Error;

// AD type "Service Data - 16-bit UUID".
constexpr uint8_t kServiceDataAdType = 0x16;
// The official client advertises with this adjusted TX power.
constexpr int8_t kAdjustedTxPower = -66;

std::optional<Error> ToError(BluetoothError error) {
  switch (error) {
    case BluetoothError::RadioNotAvailable:
      return Error::kBluetoothRadioUnavailable;
    case BluetoothError::ResourceInUse:
      return Error::kResourceInUse;
    case BluetoothError::DisabledByPolicy:
      return Error::kDisabledByPolicy;
    case BluetoothError::NotSupported:
      return Error::kHardwareNotSupported;
    case BluetoothError::DisabledByUser:
      return Error::kDisabledByUser;
    case BluetoothError::ConsentRequired:
      return Error::kConsentRequired;
    case BluetoothError::TransportNotSupported:
      return Error::kTransportNotSupported;
    case BluetoothError::Success:
    case BluetoothError::DeviceNotConnected:
      return std::nullopt;
    default:
      return Error::kUnknown;
  }
}

}  // namespace

void FastInitBleBeacon::SerializeToByteArray() {
  std::array<uint8_t, kAdvertiseDataTotalSize> bytes{};
  // The service UUID goes out little endian.
  bytes[0] = kFastInitServiceUuid[1];
  bytes[1] = kFastInitServiceUuid[0];
  std::copy(std::begin(kFastInitModelId), std::end(kFastInitModelId),
            bytes.begin() + 2);
  bytes[5] = static_cast<uint8_t>(version_) << 5 |
             (static_cast<uint8_t>(type_) & 7) << 2 | is_uwb_supported_ << 1 |
             is_sender_cert_supported_;
  bytes[6] = static_cast<uint8_t>(-adjusted_tx_power_);
  if (is_uwb_supported_) {
    bytes[7] = uwb_metadata_[0];
    std::copy(uwb_address_.begin(), uwb_address_.end(), bytes.begin() + 8);
  }
  if (is_sender_cert_supported_) {
    bytes[16] = salt_[0];
    std::copy(secret_id_hash_.begin(), secret_id_hash_.end(),
              bytes.begin() + 17);
  }
  bytes[25] = self_only_advertising_ << 6 | require_bt_advertising_ << 7;
  advertising_data_byte_array_ = bytes;
}

FastInitiationManager::~FastInitiationManager() {
  absl::MutexLock lock(mutex_);
  if (publisher_ != nullptr) {
    try {
      publisher_.Stop();
    } catch (const winrt::hresult_error& error) {
      LOG(WARNING) << "Fast Initiation publisher Stop failed: "
                   << error.code();
    }
    ReleasePublisher();
  }
}

void FastInitiationManager::StartAdvertising(
    nearby::api::FastInitBleBeacon::FastInitType type,
    std::function<void()> callback, std::function<void(Error)> error_callback) {
  absl::MutexLock lock(mutex_);
  if (is_advertising_) {
    LOG(WARNING) << __func__ << ": Fast Initiation is already advertising.";
    error_callback(Error::kResourceInUse);
    return;
  }

  FastInitBleBeacon beacon{};
  beacon.SetVersion(FastInitBleBeacon::FastInitVersion::kV1);
  beacon.SetType(type);
  beacon.SetAdjustedTxPower(kAdjustedTxPower);
  beacon.SetRequireBtAdvertising(!adapter_.IsExtendedAdvertisingSupported());
  beacon.SerializeToByteArray();

  try {
    std::array<uint8_t, FastInitBleBeacon::kAdvertiseDataTotalSize> bytes =
        beacon.GetAdDataByteArray();
    DataWriter writer;
    writer.WriteBytes(bytes);
    BluetoothLEAdvertisement advertisement;
    advertisement.DataSections().Append(BluetoothLEAdvertisementDataSection(
        kServiceDataAdType, writer.DetachBuffer()));
    if (publisher_ != nullptr) ReleasePublisher();
    publisher_ = Publisher(advertisement);
    start_callback_ = std::move(callback);
    error_callback_ = std::move(error_callback);
    status_token_ = publisher_.StatusChanged(
        {this, &FastInitiationManager::OnStatusChanged});
    publisher_.Start();
    LOG(INFO) << __func__ << ": Fast Initiation advertising requested.";
  } catch (const winrt::hresult_error& error) {
    // Not reported through error_callback: the service answers a hardware
    // error by starting again, which would retry a failing call in a loop.
    LOG(ERROR) << __func__ << ": Fast Initiation publisher failed: "
               << error.code() << ": " << winrt::to_string(error.message());
    if (publisher_ != nullptr) ReleasePublisher();
  }
}

void FastInitiationManager::StopAdvertising(std::function<void()> callback) {
  absl::MutexLock lock(mutex_);
  if (!is_advertising_) {
    callback();
    return;
  }
  stop_callback_ = std::move(callback);
  try {
    publisher_.Stop();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Fast Initiation publisher Stop failed: "
               << error.code();
    ReleasePublisher();
    std::exchange(stop_callback_, nullptr)();
  }
}

bool FastInitiationManager::IsAdvertising() {
  absl::MutexLock lock(mutex_);
  return is_advertising_;
}

void FastInitiationManager::OnStatusChanged(
    const Publisher& publisher,
    const BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args) {
  // Callbacks run after the lock is released.
  std::function<void()> done;
  std::function<void()> failed;
  {
    absl::MutexLock lock(mutex_);
    if (publisher != publisher_) return;  // From a replaced publisher.
    switch (args.Status()) {
      case BluetoothLEAdvertisementPublisherStatus::Started:
        is_advertising_ = true;
        done = start_callback_;
        LOG(INFO) << "Fast Initiation advertising started.";
        break;
      case BluetoothLEAdvertisementPublisherStatus::Stopped:
        done = std::exchange(stop_callback_, nullptr);
        ReleasePublisher();
        LOG(INFO) << "Fast Initiation advertising stopped.";
        break;
      case BluetoothLEAdvertisementPublisherStatus::Aborted: {
        BluetoothError bluetooth_error = args.Error();
        LOG(ERROR) << "Fast Initiation advertising aborted, BluetoothError="
                   << static_cast<int>(bluetooth_error);
        std::optional<Error> error = ToError(bluetooth_error);
        if (error.has_value() && error_callback_) {
          failed = [callback = error_callback_, error = *error]() {
            callback(error);
          };
        }
        ReleasePublisher();
        break;
      }
      default:
        break;
    }
  }
  if (done) done();
  if (failed) failed();
}

void FastInitiationManager::ReleasePublisher() {
  publisher_.StatusChanged(status_token_);
  publisher_ = nullptr;
  is_advertising_ = false;
}

}  // namespace nearby::sharing::windows
