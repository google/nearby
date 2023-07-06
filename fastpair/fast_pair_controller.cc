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

#include "fastpair/fast_pair_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "fastpair/common/protocol.h"
#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"
#include "fastpair/message_stream/message_stream.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

FastPairController::FastPairController(Mediums* mediums, FastPairDevice* device,
                                       SingleThreadExecutor* executor)
    : mediums_(mediums), device_(device), executor_(executor) {}

absl::Status FastPairController::OpenMessageStream() {
  message_stream_ = std::make_unique<MessageStream>(
      *device_, &mediums_->GetBluetoothClassic().GetMedium(), *this);
  return message_stream_->OpenRfcomm();
}

void FastPairController::AddMessageStreamConnectionObserver(
    MessageStreamConnectionObserver* observer) {
  connection_observers_.AddObserver(observer);
  if (message_stream_status_.ok() && observer->on_connected != nullptr) {
    observer->on_connected(*device_);
  } else if (!message_stream_status_.ok() &&
             observer->on_disconnected != nullptr) {
    observer->on_disconnected(*device_, message_stream_status_);
  }
}

void FastPairController::RemoveMessageStreamConnectionObserver(
    MessageStreamConnectionObserver* observer) {
  connection_observers_.RemoveObserver(observer);
}

void FastPairController::AddAddressRotationObserver(
    BleAddressRotationObserver* observer) {
  address_rotation_observers_.AddObserver(observer);
  if (!device_->GetBleAddress().empty()) {
    observer->on_ble_address_rotated(*device_, device_->GetBleAddress());
  }
}

void FastPairController::RemoveAddressRotationObserver(
    BleAddressRotationObserver* observer) {
  address_rotation_observers_.RemoveObserver(observer);
}

void FastPairController::AddModelIdObserver(ModelIdObserver* observer) {
  model_id_observers_.AddObserver(observer);
  if (!device_->GetModelId().empty()) {
    observer->on_model_id(*device_);
  }
}

void FastPairController::RemoveModelIdObserver(ModelIdObserver* observer) {
  model_id_observers_.RemoveObserver(observer);
}

Future<std::shared_ptr<FastPairDataEncryptor>>
FastPairController::GetDataEncryptor() {
  if (!encryptor_) {
    encryptor_ =
        std::make_unique<Future<std::shared_ptr<FastPairDataEncryptor>>>();
    if (device_->GetMetadata()) {
      CreateDataEncryptor();
    } else {
      FastPairRepository::Get()->GetDeviceMetadata(
          device_->GetModelId(),
          [this](std::optional<DeviceMetadata> metadata) {
            if (!metadata.has_value()) {
              NEARBY_LOGS(WARNING)
                  << __func__ << ": Failed to get device metadata";
              return;
            }
            device_->SetMetadata(metadata.value());
            CreateDataEncryptor();
          });
    }
  }
  return *encryptor_;
}

void FastPairController::CreateDataEncryptor() {
  FastPairDataEncryptorImpl::Factory::CreateAsync(
      *device_, [borrowable = lender_.GetBorrowable()](
                    std::unique_ptr<FastPairDataEncryptor> encryptor) mutable {
        auto borrowed = borrowable.Borrow();
        if (borrowed) {
          (*borrowed)->SetDataEncryptor(std::move(encryptor));
        }
      });
}

Future<FastPairController::GattClientRef>
FastPairController::GetGattClientRef() {
  Future<FastPairController::GattClientRef> result;
  if (gatt_client_ == nullptr) {
    gatt_client_ref_count_ = 0;
    gatt_client_ = FastPairGattServiceClientImpl::Factory::Create(
        *device_, *mediums_, executor_);
    gatt_client_->InitializeGattConnection(
        [](std::optional<PairFailure> result) {
          if (result.has_value()) {
            NEARBY_LOGS(INFO) << "InitializeGattConnection: " << result.value();
          } else {
            NEARBY_LOGS(INFO) << "InitializeGattConnection: OK";
          }
        });
  }
  result.Set(GattClientRef(this));
  return result;
}

// MessageStream::Observer callbacks
void FastPairController::OnConnectionResult(absl::Status result) {
  NEARBY_LOGS(INFO) << "OnConnectionResult " << result;
  message_stream_status_ = result;
  if (!result.ok()) {
    NEARBY_LOGS(INFO) << "Connection attempt failed with " << result;
    return;
  }
  for (auto* observer : connection_observers_.GetObservers()) {
    if (observer->on_connected != nullptr) {
      observer->on_connected(*device_);
    }
  }
}
void FastPairController::OnDisconnected(absl::Status status) {
  NEARBY_LOGS(INFO) << "OnDisconnected " << status;
  message_stream_status_ = status;
  for (auto* observer : connection_observers_.GetObservers()) {
    if (observer->on_disconnected != nullptr) {
      observer->on_disconnected(*device_, status);
    }
  }
}
void FastPairController::OnEnableSilenceMode(bool enable) {}
void FastPairController::OnLogBufferFull() {}
void FastPairController::OnModelId(absl::string_view model_id) {
  device_->SetModelId(absl::BytesToHexString(model_id));
  NEARBY_LOGS(INFO) << "Setting model id: " << device_->GetModelId();
  for (auto* observer : model_id_observers_.GetObservers()) {
    observer->on_model_id(*device_);
  }
}

void FastPairController::OnBleAddressUpdated(absl::string_view address) {
  std::string old_address = std::string(device_->GetBleAddress());
  device_->SetBleAddress(address);
  for (auto* observer : address_rotation_observers_.GetObservers()) {
    observer->on_ble_address_rotated(*device_, old_address);
  }
}

void FastPairController::OnBatteryUpdated(
    std::vector<MessageStream::BatteryInfo> battery_levels) {}
void FastPairController::OnRemainingBatteryTime(absl::Duration duration) {}
bool FastPairController::OnRing(uint8_t components, absl::Duration duration) {
  return false;
}

}  // namespace fastpair
}  // namespace nearby
