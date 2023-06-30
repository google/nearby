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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_gatt_service_client_impl.h"
#include "fastpair/message_stream/message_stream.h"
#include "internal/base/observer_list.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

class FastPairController : public MessageStream::Observer {
 public:
  struct MessageStreamConnectionObserver {
    // Callback triggered when a Message Stream connection is opened.
    absl::AnyInvocable<void(const FastPairDevice& device)> on_connected;

    // Callback triggered when a Message Stream connection is terminated.
    absl::AnyInvocable<void(const FastPairDevice& device, absl::Status status)>
        on_disconnected;
  };
  struct BleAddressRotationObserver {
    absl::AnyInvocable<void(const FastPairDevice& device,
                            absl::string_view old_address)>
        on_ble_address_rotated;
  };
  struct ModelIdObserver {
    absl::AnyInvocable<void(const FastPairDevice& device)> on_model_id;
  };
  // RAII accessor to GattClient
  class GattClientRef {
   public:
    GattClientRef() = default;
    explicit GattClientRef(FastPairController* controller)
        : controller_(controller) {
      CHECK(controller_ != nullptr);
      controller_->AcquireGattClient();
    }
    GattClientRef(const GattClientRef& other) {
      controller_ = other.controller_;
      if (controller_ != nullptr) {
        controller_->AcquireGattClient();
      }
    }
    GattClientRef(GattClientRef&& other) {
      controller_ = other.controller_;
      other.controller_ = nullptr;
    }
    GattClientRef& operator=(const GattClientRef& other) {
      if (controller_ != nullptr) {
        controller_->ReleaseGattClient();
      }
      controller_ = other.controller_;
      if (controller_ != nullptr) {
        controller_->AcquireGattClient();
      }
      return *this;
    }
    GattClientRef& operator=(GattClientRef&& other) {
      if (controller_ != nullptr) {
        controller_->ReleaseGattClient();
      }
      controller_ = other.controller_;
      other.controller_ = nullptr;
      return *this;
    }

    ~GattClientRef() {
      if (controller_ != nullptr) {
        controller_->ReleaseGattClient();
      }
    }

    FastPairGattServiceClient* operator->() {
      CHECK(controller_ != nullptr);
      return controller_->GetGattClient();
    }

   private:
    FastPairController* controller_ = nullptr;
  };
  // Creates a device controller in retroactive pairing path.
  FastPairController(Mediums* mediums, FastPairDevice* device,
                     SingleThreadExecutor* executor);

  ~FastPairController() override {
    NEARBY_LOGS(INFO) << "Destroy FastPairController";
    lender_.Release();
    connection_observers_.Clear();
    address_rotation_observers_.Clear();
    model_id_observers_.Clear();
    message_stream_.reset();
    NEARBY_LOGS(INFO) << "Destroyed FastPairController";
  }

  absl::Status OpenMessageStream();

  // Registers an observer for connection events. The observer must stay alive
  // until it is removed.
  // The callback is also called during registration with the current state
  // of the connection.
  void AddMessageStreamConnectionObserver(
      MessageStreamConnectionObserver* observer);
  void RemoveMessageStreamConnectionObserver(
      MessageStreamConnectionObserver* observer);

  // Registers an observer for BLE address rotation.
  // The callback is also called during registration with the current BLE
  // address unless FP does not know the device's address.
  void AddAddressRotationObserver(BleAddressRotationObserver* observer);
  void RemoveAddressRotationObserver(BleAddressRotationObserver* observer);

  void AddModelIdObserver(ModelIdObserver* observer);
  void RemoveModelIdObserver(ModelIdObserver* observer);

  Future<std::shared_ptr<FastPairDataEncryptor>> GetDataEncryptor();
  Future<GattClientRef> GetGattClientRef();

  // MessageStream::Observer callbacks
  void OnConnectionResult(absl::Status result) override;
  void OnDisconnected(absl::Status status) override;
  void OnEnableSilenceMode(bool enable) override;
  void OnLogBufferFull() override;
  void OnModelId(absl::string_view model_id) override;
  void OnBleAddressUpdated(absl::string_view address) override;
  void OnBatteryUpdated(
      std::vector<MessageStream::BatteryInfo> battery_levels) override;
  void OnRemainingBatteryTime(absl::Duration duration) override;
  bool OnRing(uint8_t components, absl::Duration duration) override;

  FastPairDevice& GetDevice() { return *device_; }
  std::string GetSeekerMacAddress() const {
    return mediums_->GetBluetoothClassic().GetMedium().GetMacAddress();
  }

 private:
  void SetDataEncryptor(std::unique_ptr<FastPairDataEncryptor> encryptor) {
    if (encryptor != nullptr) {
      encryptor_->Set(std::move(encryptor));
    } else {
      encryptor_->SetException({Exception::kFailed});
    }
  }
  void AcquireGattClient() { ++gatt_client_ref_count_; }
  void ReleaseGattClient() {
    if (--gatt_client_ref_count_ == 0) {
      gatt_client_.reset();
    }
  }
  FastPairGattServiceClient* GetGattClient() {
    CHECK(gatt_client_ != nullptr);
    return gatt_client_.get();
  }
  void CreateDataEncryptor();
  Mediums* mediums_;
  FastPairDevice* device_;
  SingleThreadExecutor* executor_;
  std::unique_ptr<Future<std::shared_ptr<FastPairDataEncryptor>>> encryptor_;
  std::unique_ptr<MessageStream> message_stream_;
  absl::Status message_stream_status_ =
      absl::FailedPreconditionError("not connected");
  ObserverList<MessageStreamConnectionObserver> connection_observers_;
  ObserverList<BleAddressRotationObserver> address_rotation_observers_;
  ObserverList<ModelIdObserver> model_id_observers_;
  // TODO(jsobczak): Improve locking around gatt_client_
  std::unique_ptr<FastPairGattServiceClient> gatt_client_;
  std::atomic_int gatt_client_ref_count_;
  Lender<FastPairController*> lender_{this};
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_CONTROLLER_H_
