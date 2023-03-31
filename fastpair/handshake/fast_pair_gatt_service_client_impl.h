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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"
#include "fastpair/internal/ble/ble.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/timer_impl.h"

namespace nearby {
namespace fastpair {
using GattCharacteristic = api::ble_v2::GattCharacteristic;

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClientImpl : public FastPairGattServiceClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairGattServiceClient> Create(
        const FastPairDevice& device);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairGattServiceClient> CreateInstance() = 0;

   private:
    static Factory* g_test_factory_;
  };

  explicit FastPairGattServiceClientImpl(const FastPairDevice& device);
  FastPairGattServiceClientImpl(const FastPairGattServiceClientImpl&) = delete;
  FastPairGattServiceClientImpl& operator=(
      const FastPairGattServiceClientImpl&) = delete;
  ~FastPairGattServiceClientImpl() override = default;

  void InitializeGattConnection(
      absl::AnyInvocable<void(std::optional<PairFailure>)>
          on_gatt_initialized_callback) override;

  void WriteRequestAsync(
      uint8_t message_type, uint8_t flags, absl::string_view provider_address,
      absl::string_view seekers_address,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) override;

  void WritePasskeyAsync(
      uint8_t message_type, uint32_t passkey,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) override;

 private:
  // Attempt to create a GATT connection with the device. This method may be
  // called multiple times.
  void AttemptGattConnection();
  void CreateGattConnection();
  void DiscoverServiceAndCharacteristics();
  void GetFastPairGattCharacteristics();
  std::optional<GattCharacteristic> GetCharacteristicsByUUIDs(
      const Uuid& uuidV1, const Uuid& uuidV2);

  // Operations on KeyBased Characteristic
  // Creates a data vector based on parameter information.
  std::array<uint8_t, kAesBlockByteSize> CreateRequest(
      uint8_t message_type, uint8_t flags, absl::string_view provider_address,
      absl::string_view seekers_address);
  // Subscribe notification when KeyBased Characteristic value changes
  bool SubscribeKeyBasedCharacteristic();
  // Write request to KeyBased Characteristic
  void WriteKeyBasedCharacteristic(absl::string_view request);

  // Operations on Passkey Characteristic
  // Creates a data vector based on parameter information.
  std::array<uint8_t, kAesBlockByteSize> CreatePasskeyBlock(
      uint8_t message_type, uint32_t passkey);
  // Subscribe notification when Passkey Characteristic value changes
  bool SubscribePasskeyCharacteristic();
  // Write request to Passkey Characteristic
  void WritePasskeyCharacteristic(absl::string_view request);

  // Callback is triggered when characteristic value changes
  void OnCharacteristicValueChanged(const GattCharacteristic& characteristic,
                                    absl::string_view value);

  // Invokes the initialized callback with the proper PairFailure and clears
  // local state.
  void NotifyInitializedError(PairFailure failure);
  // Invokes the write response callback with the proper PairFailure on a
  // write error.
  void NotifyWriteRequestError(PairFailure failure);
  void NotifyWritePasskeyError(PairFailure failure);

  void ClearCurrentState();

  // Timers
  TimerImpl gatt_service_discovery_timer_;
  TimerImpl key_based_subscription_timer_;
  TimerImpl passkey_subscription_timer_;
  TimerImpl key_based_write_request_timer_;
  TimerImpl passkey_write_request_timer_;

  void OnGattServiceDiscoveryTimeout();

  // Callback
  absl::AnyInvocable<void(std::optional<PairFailure>)>
      on_gatt_initialized_callback_;
  WriteResponseCallback key_based_write_response_callback_;
  WriteResponseCallback passkey_write_response_callback_;

  // Fast Pair Characteristic
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;

  bool is_key_based_notification_subscribed_ = false;
  bool is_passkey_notification_subscribed_ = false;

  // Initialize with zero failures.
  int num_gatt_connection_attempts_ = 0;

  bool is_initialized_ = false;
  std::string device_address_;
  std::unique_ptr<GattClient> gatt_client_;
  Ble ble_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
