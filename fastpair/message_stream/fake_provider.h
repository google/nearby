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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_PROVIDER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_PROVIDER_H_

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/message_stream/fake_gatt_callbacks.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/ble.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/uuid.h"
#include <openssl/base.h>
#include <openssl/evp.h>

namespace nearby {
namespace fastpair {

// Fake BT device with the Provider role. Tailored to testing message stream.
class FakeProvider {
  using Property = nearby::api::ble_v2::GattCharacteristic::Property;
  using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
  using GattCharacteristic = nearby::api::ble_v2::GattCharacteristic;
  using WriteType = nearby::api::ble_v2::GattClient::WriteType;

  static constexpr inline Uuid kFastPairServiceUuid{0x0000FE2C00001000,
                                                    0x800000805F9B34FB};
  static constexpr inline Uuid kKeyBasedCharacteristicUuidV2{
      0xFE2C123483664814, 0x8EB001DE32100BEA};
  static constexpr inline Uuid kPasskeyCharacteristicUuidV2{0xFE2C123583664814,
                                                            0x8EB001DE32100BEA};
  static constexpr inline Uuid kAccountKeyCharacteristicUuidV2{
      0xFE2C123683664814, 0x8EB001DE32100BEA};
  static constexpr inline absl::string_view kServiceID = "Fast Pair";

  static constexpr inline absl::string_view
      kKeyBasedCharacteristicAdvertisementByte = "keyBasedCharacte";
  static constexpr inline absl::string_view
      kPasskeyharacteristicAdvertisementByte = "passkeyCharacter";

 public:
  using KeyBasedPairingCallback =
      absl::AnyInvocable<std::string(absl::string_view)>;
  struct PairingConfig {
    std::string private_key;  // binary, private Anti-Spoofing Key
    std::string public_key;   // binary, public Anti-Spoofing Key
    std::string model_id;
    std::string pass_key;
  };
  ~FakeProvider() { Shutdown(); }

  // Sets the fake provider up for initial pairing
  void PrepareForInitialPairing(PairingConfig config,
                                FakeGattCallbacks* fake_gatt_callbacks);

  // Sets the fake provider up for retroactive pairing
  void PrepareForRetroactivePairing(PairingConfig config,
                                    FakeGattCallbacks* fake_gatt_callbacks);

  void Shutdown() {
    StopAdvertising();
    provider_thread_.Shutdown();
  }

  void DiscoverProvider(BluetoothClassicMedium& seeker_medium) {
    CountDownLatch found_latch(1);
    seeker_medium.StartDiscovery(BluetoothClassicMedium::DiscoveryCallback{
        .device_discovered_cb =
            [&](BluetoothDevice& device) {
              NEARBY_LOG(INFO, "Device discovered: %s",
                         device.GetName().c_str());
              found_latch.CountDown();
            },
    });
    provider_adapter_.SetScanMode(
        BluetoothAdapter::ScanMode::kConnectableDiscoverable);
    ASSERT_TRUE(found_latch.Await().Ok());
  }

  void EnableProviderRfcomm() {
    std::string service_name{"service"};
    std::string uuid(kRfcommUuid);
    provider_server_socket_ =
        provider_medium_.ListenForService(service_name, uuid);
    provider_thread_.Execute(
        [this]() { provider_socket_ = provider_server_socket_.Accept(); });
  }

  Future<std::string> ReadProviderBytes(size_t num_bytes) {
    Future<std::string> result;
    provider_thread_.Execute([this, result, num_bytes]() mutable {
      if (!provider_socket_.IsValid()) {
        result.SetException({Exception::kIo});
        return;
      }
      ExceptionOr<ByteArray> bytes =
          provider_socket_.GetInputStream().Read(num_bytes);
      if (bytes.ok()) {
        result.Set(std::string(bytes.GetResult().AsStringView()));
      } else {
        result.SetException(bytes.GetException());
      }
    });
    return result;
  }

  void WriteProviderBytes(std::string bytes) {
    CountDownLatch latch(1);
    provider_thread_.Execute([&, data = ByteArray(bytes)]() {
      if (provider_socket_.IsValid()) {
        provider_socket_.GetOutputStream().Write(data);
      }
      latch.CountDown();
    });
    latch.Await();
  }

  void DisableProviderRfcomm() {
    if (provider_server_socket_.IsValid()) {
      provider_server_socket_.Close();
    }
    provider_thread_.Execute([this]() {
      if (provider_socket_.IsValid()) {
        provider_socket_.Close();
      }
    });
  }

  std::string GetMacAddress() const {
    return provider_adapter_.GetMacAddress();
  }

  std::string GetMacAddressAsBytes() const {
    return std::string(
        BluetoothUtils::FromString(provider_adapter_.GetMacAddress()));
  }

  void StartGattServer(FakeGattCallbacks* fake_gatt_callbacks);

  void InsertCorrectGattCharacteristics() {
    CHECK_NE(fake_gatt_callbacks_, nullptr);

    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    CHECK(key_based_characteristic_.has_value());
    fake_gatt_callbacks_->characteristics_[*key_based_characteristic_]
        .write_result = absl::OkStatus();

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    CHECK(passkey_characteristic_.has_value());
    fake_gatt_callbacks_->characteristics_[*passkey_characteristic_]
        .write_result = absl::OkStatus();

    accountkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kAccountKeyCharacteristicUuidV2, permissions_,
        properties_);
    CHECK(accountkey_characteristic_.has_value());
    fake_gatt_callbacks_->characteristics_[*accountkey_characteristic_]
        .write_result = absl::OkStatus();
  }

  void LoadAntiSpoofingKey(absl::string_view private_key,
                           absl::string_view public_key);

  std::string DecryptKbpRequest(absl::string_view request);
  std::string Encrypt(absl::string_view data);

  absl::Status NotifyKeyBasedPairing(ByteArray response);
  absl::Status NotifyPasskey(ByteArray response);

  void StartDiscoverableAdvertisement(absl::string_view model_id);
  void StopAdvertising();
  void ConfigurePairingContext(absl::string_view pass_key);
  AccountKey& GetAccountKey() { return account_key_; }
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;

 private:
  void SetKeyBasedPairingCallback();
  void SetPasskeyCallback();
  void SetAccountkeyCallback();
  void SetPairedStatus(bool paired);
  void EnableProviderRfcommForRetro(PairingConfig& config);
  ByteArray GetModelIdMessage(absl::string_view model_id);
  ByteArray GetBleAddressMessage();
  std::string GenSec256r1Secret(absl::string_view remote_party_public_key);
  std::string CreateSharedSecret(absl::string_view remote_public_key);
  BluetoothAdapter provider_adapter_;
  BluetoothClassicMedium provider_medium_{provider_adapter_};
  BluetoothServerSocket provider_server_socket_;
  BluetoothSocket provider_socket_;
  BleV2Medium ble_{provider_adapter_};
  BleMedium ble_v1_{provider_adapter_};
  bool advertising_ = false;
  std::unique_ptr<GattServer> gatt_server_;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
  std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)> anti_spoofing_key_{
      nullptr, EVP_PKEY_free};
  std::string shared_secret_;
  SingleThreadExecutor provider_thread_;
  std::string model_id_;
  FakeGattCallbacks* fake_gatt_callbacks_ = nullptr;
  unsigned int pass_key_;
  AccountKey account_key_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_PROVIDER_H_
