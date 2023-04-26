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

#include "fastpair/handshake/fast_pair_gatt_service_client_impl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include <openssl/rand.h>

namespace nearby {
namespace fastpair {

namespace {
// We have two UUID possibilities for each characteristic because they changed
// across different Fast Pair versions.
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV1(0x0000123400001000,
                                             0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV1(0x0000123500001000,
                                            0x800000805F9B34FB);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
                                            0x8EB001DE32100BEA);

constexpr absl::Duration kGattOperationTimeout = absl::Seconds(15);
constexpr int kMaxNumGattConnectionAttempts = 3;
}  // namespace

// FastPairGattServiceClientImpl::Factory
// static
FastPairGattServiceClientImpl::Factory*
    FastPairGattServiceClientImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairGattServiceClient>
FastPairGattServiceClientImpl::Factory::Create(const FastPairDevice& device) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance();
  }

  return std::make_unique<FastPairGattServiceClientImpl>(device);
}

// static
void FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairGattServiceClientImpl::Factory::~Factory() = default;

FastPairGattServiceClientImpl::FastPairGattServiceClientImpl(
    const FastPairDevice& device)
    : device_address_(device.GetBleAddress()) {}

void FastPairGattServiceClientImpl::InitializeGattConnection(
    absl::AnyInvocable<void(std::optional<PairFailure>)>
        on_gatt_initialized_callback) {
  NEARBY_LOGS(INFO) << __func__
                    << ": Starting the GATT connection to the device.";
  on_gatt_initialized_callback_ = std::move(on_gatt_initialized_callback);
  AttemptGattConnection();
}

void FastPairGattServiceClientImpl::AttemptGattConnection() {
  NEARBY_LOGS(INFO) << __func__ << ": Attempt to connect to the device.";
  if (num_gatt_connection_attempts_ == kMaxNumGattConnectionAttempts) {
    NotifyInitializedError(PairFailure::kCreateGattConnection);
    return;
  }
  num_gatt_connection_attempts_++;
  NEARBY_LOGS(INFO) << __func__ << ": Starting GATT connection attempt #"
                    << num_gatt_connection_attempts_ << " to device";

  if (gatt_client_) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Disconnecting previous connections before attempt";
    gatt_client_->Disconnect();
    gatt_client_ = nullptr;
  }
  CreateGattConnection();
}

void FastPairGattServiceClientImpl::CreateGattConnection() {
  NEARBY_LOGS(INFO) << __func__ << " : Create Gatt Connection to the device.";
  gatt_client_ = ble_.ConnectToGattServer(device_address_);
  if (!gatt_client_) {
    // The device must have been lost between connection attempts.
    NotifyInitializedError(
        PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts);
    return;
  }
  DiscoverServiceAndCharacteristics();
}

void FastPairGattServiceClientImpl::DiscoverServiceAndCharacteristics() {
  NEARBY_LOGS(INFO) << __func__
                    << " : Start to discovery servie and characteristic.";
  gatt_service_discovery_timer_.Start(
      kGattOperationTimeout / absl::Milliseconds(1), 0,
      [&]() { OnGattServiceDiscoveryTimeout(); });

  if (gatt_client_->DiscoverServiceAndCharacteristics(
          kFastPairServiceUuid,
          {kKeyBasedCharacteristicUuidV2, kPasskeyCharacteristicUuidV2}) ||
      gatt_client_->DiscoverServiceAndCharacteristics(
          kFastPairServiceUuid,
          {kKeyBasedCharacteristicUuidV1, kPasskeyCharacteristicUuidV1})) {
    gatt_service_discovery_timer_.Stop();
    NEARBY_LOGS(INFO) << __func__
                      << ": Completed discovery for Fast Pair GATT service and "
                         "characterisitc.";
    GetFastPairGattCharacteristics();
    return;
  }
  NEARBY_LOGS(INFO) << __func__
                    << ": Failed to discovery for Fast Pair GATT service and "
                       "characterisitc."
                    << PairFailure::kGattServiceDiscovery;
  AttemptGattConnection();
}

void FastPairGattServiceClientImpl::GetFastPairGattCharacteristics() {
  NEARBY_LOGS(INFO) << __func__ << " :Start to get Fast Pair characteristic.";
  key_based_characteristic_ = GetCharacteristicsByUUIDs(
      kKeyBasedCharacteristicUuidV1, kKeyBasedCharacteristicUuidV2);
  if (!key_based_characteristic_.has_value()) {
    NotifyInitializedError(
        PairFailure::kKeyBasedPairingCharacteristicDiscovery);
    return;
  }

  passkey_characteristic_ = GetCharacteristicsByUUIDs(
      kPasskeyCharacteristicUuidV1, kPasskeyCharacteristicUuidV2);
  if (!passkey_characteristic_.has_value()) {
    NotifyInitializedError(PairFailure::kPasskeyCharacteristicDiscovery);
    return;
  }

  is_initialized_ = true;
  std::move(on_gatt_initialized_callback_)(std::nullopt);
}

std::optional<GattCharacteristic>
FastPairGattServiceClientImpl::GetCharacteristicsByUUIDs(const Uuid& uuidV1,
                                                         const Uuid& uuidV2) {
  // Default to V2 device to match Android implementation.
  std::optional<GattCharacteristic> characteristics =
      gatt_client_->GetCharacteristic(kFastPairServiceUuid, uuidV2);
  if (characteristics.has_value()) {
    return characteristics;
  }
  return gatt_client_->GetCharacteristic(kFastPairServiceUuid, uuidV1);
}

void FastPairGattServiceClientImpl::OnGattServiceDiscoveryTimeout() {
  NEARBY_LOGS(INFO) << __func__
                    << ": reattempting from previous GATT connection failure: "
                    << PairFailure::kGattServiceDiscoveryTimeout;
  AttemptGattConnection();
}

std::array<uint8_t, kAesBlockByteSize>
FastPairGattServiceClientImpl::CreateRequest(
    uint8_t message_type, uint8_t flags, absl::string_view provider_address,
    absl::string_view seekers_address) {
  std::array<uint8_t, kAesBlockByteSize> data_to_write;
  RAND_bytes(data_to_write.data(), kAesBlockByteSize);

  data_to_write[0] = message_type;
  data_to_write[1] = flags;

  std::array<uint8_t, 6> provider_address_bytes;

  device::ParseBluetoothAddress(provider_address,
                                absl::MakeSpan(provider_address_bytes.data(),
                                               provider_address_bytes.size()));
  std::copy(provider_address_bytes.begin(), provider_address_bytes.end(),
            std::begin(data_to_write) + kProviderAddressStartIndex);

  // Seekers address can be empty, in which we would just have the bytes be
  // the salt.
  if (!seekers_address.empty()) {
    std::array<uint8_t, 6> seeker_address_bytes;
    device::ParseBluetoothAddress(seekers_address,
                                  absl::MakeSpan(seeker_address_bytes.data(),
                                                 seeker_address_bytes.size()));
    std::copy(seeker_address_bytes.begin(), seeker_address_bytes.end(),
              std::begin(data_to_write) + kSeekerAddressStartIndex);
  }

  return data_to_write;
}

std::array<uint8_t, kAesBlockByteSize>
FastPairGattServiceClientImpl::CreatePasskeyBlock(uint8_t message_type,
                                                  uint32_t passkey) {
  std::array<uint8_t, kAesBlockByteSize> data_to_write;
  RAND_bytes(data_to_write.data(), kAesBlockByteSize);

  data_to_write[0] = message_type;

  // Need to convert the uint_32 to uint_8 to use in our data vector.
  data_to_write[1] = (passkey & 0x00ff0000) >> 16;
  data_to_write[2] = (passkey & 0x0000ff00) >> 8;
  data_to_write[3] = passkey & 0x000000ff;
  return data_to_write;
}

void FastPairGattServiceClientImpl::WriteRequestAsync(
    uint8_t message_type, uint8_t flags, absl::string_view provider_address,
    absl::string_view seekers_address,
    const FastPairDataEncryptor& fast_pair_data_encryptor,
    WriteResponseCallback callback) {
  DCHECK(is_initialized_);
  DCHECK(!key_based_write_response_callback_);
  // The key based request should only ever be written once
  DCHECK(!is_key_based_notification_subscribed_);

  key_based_write_response_callback_ = std::move(callback);

  const std::array<uint8_t, kAesBlockByteSize> data_to_write =
      fast_pair_data_encryptor.EncryptBytes(CreateRequest(
          message_type, flags, provider_address, seekers_address));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

  // Append the public version of the private key to the message so thedevice
  // can generate the shared secret to decrypt the message.
  const std::optional<std::array<uint8_t, 64>> public_key =
      fast_pair_data_encryptor.GetPublicKey();

  if (public_key) {
    const std::vector<uint8_t> public_key_vec = std::vector<uint8_t>(
        public_key.value().begin(), public_key.value().end());
    data_to_write_vec.insert(data_to_write_vec.end(), public_key_vec.begin(),
                             public_key_vec.end());
  }
  // Subscribe the notification once the keybased characteristic's value changed
  if (SubscribeKeyBasedCharacteristic()) {
    is_key_based_notification_subscribed_ = true;
    // Write public address request to the keybased characteristic
    WriteKeyBasedCharacteristic(
        std::string(data_to_write_vec.begin(), data_to_write_vec.end()));
  }
}

bool FastPairGattServiceClientImpl::SubscribeKeyBasedCharacteristic() {
  NEARBY_LOGS(INFO) << __func__
                    << " :Start to subscribe notification "
                       "once keybased characteristic changed.";
  key_based_subscription_timer_.Start(
      kGattOperationTimeout / absl::Milliseconds(1), 0,
      absl::bind_front(
          &FastPairGattServiceClientImpl::NotifyWriteRequestError, this,
          PairFailure::kKeyBasedPairingCharacteristicSubscriptionTimeout));

  if (gatt_client_->SetCharacteristicSubscription(
          key_based_characteristic_.value(), true,
          [this](absl::string_view value) {
            FastPairGattServiceClientImpl::OnCharacteristicValueChanged(
                key_based_characteristic_.value(), value);
          })) {
    key_based_subscription_timer_.Stop();
    NEARBY_LOGS(INFO)
        << __func__ << ": Successfully subscribe the key based characteristic.";
    return true;
  }
  NEARBY_LOGS(INFO) << __func__
                    << ": Failed to subscribe the key based characteristic.";
  NotifyWriteRequestError(
      PairFailure::kKeyBasedPairingCharacteristicSubscription);
  return false;
}

void FastPairGattServiceClientImpl::WriteKeyBasedCharacteristic(
    absl::string_view request) {
  NEARBY_LOGS(INFO) << __func__ << " :Start to write keybased characteristic.";
  key_based_write_request_timer_.Start(
      kGattOperationTimeout / absl::Milliseconds(1), 0,
      absl::bind_front(&FastPairGattServiceClientImpl::NotifyWriteRequestError,
                       this, PairFailure::kKeyBasedPairingResponseTimeout));

  if (gatt_client_->WriteCharacteristic(
          key_based_characteristic_.value(), request,
          api::ble_v2::GattClient::WriteType::kWithResponse)) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Successfully write the key basedcharacteristic.";
    return;
  }
  NEARBY_LOGS(INFO) << __func__
                    << ": Failed to write the key based characteristic ";
  NotifyWriteRequestError(PairFailure::kKeyBasedPairingCharacteristicWrite);
}

void FastPairGattServiceClientImpl::WritePasskeyAsync(
    uint8_t message_type, uint32_t passkey,
    const FastPairDataEncryptor& fast_pair_data_encryptor,
    WriteResponseCallback callback) {
  DCHECK(is_initialized_);
  DCHECK(message_type == kSeekerPasskey);

  passkey_write_response_callback_ = std::move(callback);
  const std::array<uint8_t, kAesBlockByteSize> data_to_write =
      fast_pair_data_encryptor.EncryptBytes(
          CreatePasskeyBlock(message_type, passkey));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

  // Subscribe the notification once the passkey characteristic's value changed
  if (SubscribePasskeyCharacteristic()) {
    is_passkey_notification_subscribed_ = true;
    // Write passkey confirmation request to the passkey characteristic
    WritePasskeyCharacteristic(
        std::string(data_to_write_vec.begin(), data_to_write_vec.end()));
  }
}

bool FastPairGattServiceClientImpl::SubscribePasskeyCharacteristic() {
  NEARBY_LOGS(INFO) << __func__
                    << " :Start to subscribe notification "
                       "once passkey characteristic changed.";
  passkey_subscription_timer_.Start(
      kGattOperationTimeout / absl::Milliseconds(1), 0,
      absl::bind_front(&FastPairGattServiceClientImpl::NotifyWritePasskeyError,
                       this,
                       PairFailure::kPasskeyCharacteristicSubscriptionTimeout));

  if (gatt_client_->SetCharacteristicSubscription(
          passkey_characteristic_.value(), true,
          [this](absl::string_view value) {
            FastPairGattServiceClientImpl::OnCharacteristicValueChanged(
                passkey_characteristic_.value(), value);
          })) {
    passkey_subscription_timer_.Stop();
    NEARBY_LOGS(INFO) << __func__
                      << ": Successfully subscribe the passkey characteristic.";
    return true;
  }
  NEARBY_LOGS(INFO) << __func__
                    << ": Failed to subscribe the passkey characteristic.";
  NotifyWritePasskeyError(PairFailure::kPasskeyCharacteristicSubscription);
  return false;
}

void FastPairGattServiceClientImpl::WritePasskeyCharacteristic(
    absl::string_view request) {
  passkey_write_request_timer_.Start(
      kGattOperationTimeout / absl::Milliseconds(1), 0,
      absl::bind_front(&FastPairGattServiceClientImpl::NotifyWritePasskeyError,
                       this, PairFailure::kPasskeyResponseTimeout));
  if (gatt_client_->WriteCharacteristic(
          passkey_characteristic_.value(), request,
          api::ble_v2::GattClient::WriteType::kWithResponse)) {
    NEARBY_LOGS(INFO) << __func__
                      << ": Successfully write the passkey characteristic.";
    return;
  }
  NEARBY_LOGS(INFO) << __func__
                    << ": Failed to write the passkey characteristic ";
  NotifyWritePasskeyError(PairFailure::kPasskeyPairingCharacteristicWrite);
}

void FastPairGattServiceClientImpl::OnCharacteristicValueChanged(
    const GattCharacteristic& characteristic, absl::string_view value) {
  // We check that the callbacks still exists still before we run the
  // it with the response bytes to handle the case where the callback
  // has already been used to notify error. This can happen if the timer for
  // fires with an error, and then the write completes successfully after and
  // we get response bytes here.
  if (characteristic == key_based_characteristic_.value() &&
      key_based_write_response_callback_) {
    key_based_write_request_timer_.Stop();
    NEARBY_LOGS(INFO) << __func__
                      << ": key based characteristic value changed.";
    std::move(key_based_write_response_callback_)(value,
                                                  /*failure=*/std::nullopt);
  } else if (characteristic == passkey_characteristic_.value() &&
             passkey_write_response_callback_) {
    passkey_write_request_timer_.Stop();
    NEARBY_LOGS(INFO) << __func__ << ": Passkey characteristic value changed.";
    std::move(passkey_write_response_callback_)(value,
                                                /*failure=*/std::nullopt);
  }
}

void FastPairGattServiceClientImpl::NotifyInitializedError(
    PairFailure failure) {
  NEARBY_LOGS(VERBOSE) << __func__ << failure;
  ClearCurrentState();

  if (on_gatt_initialized_callback_) {
    NEARBY_LOGS(VERBOSE) << __func__ << "Executing initialized callback";
    std::move(on_gatt_initialized_callback_)(failure);
  }
}

void FastPairGattServiceClientImpl::NotifyWriteRequestError(
    PairFailure failure) {
  NEARBY_LOGS(VERBOSE) << __func__ << "NotifyWriteRequestError";
  key_based_write_request_timer_.Stop();
  DCHECK(key_based_write_response_callback_);
  std::move(key_based_write_response_callback_)("", failure);
}

void FastPairGattServiceClientImpl::NotifyWritePasskeyError(
    PairFailure failure) {
  NEARBY_LOGS(VERBOSE) << __func__ << "NotifyWritePasskeyError";
  passkey_write_request_timer_.Stop();
  DCHECK(passkey_write_response_callback_);
  std::move(passkey_write_response_callback_)("", failure);
}

void FastPairGattServiceClientImpl::ClearCurrentState() {
  gatt_client_.reset();
  key_based_characteristic_ = std::nullopt;
  passkey_characteristic_ = std::nullopt;
  gatt_service_discovery_timer_.Stop();
  passkey_subscription_timer_.Stop();
  key_based_subscription_timer_.Stop();
  passkey_write_request_timer_.Stop();
  key_based_write_request_timer_.Stop();
}

}  // namespace fastpair
}  // namespace nearby
