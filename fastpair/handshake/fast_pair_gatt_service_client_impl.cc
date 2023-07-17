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
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"
#include "fastpair/internal/mediums/mediums.h"
#include "internal/base/bluetooth_address.h"
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
constexpr Uuid kAccountKeyCharacteristicUuidV1(0x0000123600001000,
                                               0x800000805F9B34FB);
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);

constexpr int kKeyBasedCharacteristicIndex = 0;
constexpr int kPasskeyCharacteristicIndex = 1;
constexpr int kAccountKeyCharacteristicIndex = 2;
constexpr int kNumCharacteristics = 3;

constexpr absl::Duration kGattOperationTimeout = absl::Seconds(15);
constexpr int kMaxNumGattConnectionAttempts = 3;
}  // namespace

// FastPairGattServiceClientImpl::Factory
// static
FastPairGattServiceClientImpl::Factory*
    FastPairGattServiceClientImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairGattServiceClient>
FastPairGattServiceClientImpl::Factory::Create(const FastPairDevice& device,
                                               Mediums& mediums,
                                               SingleThreadExecutor* executor) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance();
  }

  return std::make_unique<FastPairGattServiceClientImpl>(device, mediums,
                                                         executor);
}

// static
void FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairGattServiceClientImpl::Factory::~Factory() = default;

FastPairGattServiceClientImpl::FastPairGattServiceClientImpl(
    const FastPairDevice& device, Mediums& mediums,
    SingleThreadExecutor* executor)
    : device_address_(device.GetBleAddress()),
      mediums_(mediums),
      executor_(executor) {
  gatt_connection_params_.service_uuid = kFastPairServiceUuid;
  gatt_connection_params_.characteristic_uuids.resize(kNumCharacteristics);
  gatt_connection_params_.characteristic_uuids[kKeyBasedCharacteristicIndex] = {
      kKeyBasedCharacteristicUuidV2, kKeyBasedCharacteristicUuidV1};
  gatt_connection_params_.characteristic_uuids[kPasskeyCharacteristicIndex] = {
      kPasskeyCharacteristicUuidV2, kPasskeyCharacteristicUuidV1};
  gatt_connection_params_.characteristic_uuids[kAccountKeyCharacteristicIndex] =
      {kAccountKeyCharacteristicUuidV2, kAccountKeyCharacteristicUuidV1};
}

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
  ClearCurrentState();
  CreateGattConnection();
}

void FastPairGattServiceClientImpl::CreateGattConnection() {
  NEARBY_LOGS(INFO) << __func__ << " : Create Gatt Connection to the device.";
  MutexLock lock(&mutex_);
  if (mediums_.GetBluetoothRadio().Enable() &&
      mediums_.GetBleV2().IsAvailable()) {
    gatt_client_ = mediums_.GetBleV2().ConnectToGattServer(
        device_address_, gatt_connection_params_, [this](absl::Status status) {
          NEARBY_LOGS(INFO) << "Gatt connection status: " << status;
          if (status.ok()) {
            executor_->Execute("init-success", [this]() {
              if (on_gatt_initialized_callback_) {
                std::move(on_gatt_initialized_callback_)(std::nullopt);
              }
            });
          } else {
            NotifyInitializedError(PairFailure::kCreateGattConnection);
          }
        });
  }
  if (gatt_client_) {
    is_initialized_ = true;
  } else {
    // The device must have been lost between connection attempts.
    NotifyInitializedError(
        PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts);
    return;
  }
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

std::array<uint8_t, kAesBlockByteSize>
FastPairGattServiceClientImpl::CreateAccountKeyBlock() {
  std::array<uint8_t, 16> account_key;
  RAND_bytes(account_key.data(), account_key.size());
  account_key[0] = 0x04;
  return account_key;
}

void FastPairGattServiceClientImpl::WriteRequestAsync(
    uint8_t message_type, uint8_t flags, absl::string_view provider_address,
    absl::string_view seekers_address,
    const FastPairDataEncryptor& fast_pair_data_encryptor,
    WriteResponseCallback callback) {
  CHECK(!key_based_write_response_callback_);

  key_based_write_response_callback_ = std::move(callback);

  const std::array<uint8_t, kAesBlockByteSize> data_to_write =
      fast_pair_data_encryptor.EncryptBytes(CreateRequest(
          message_type, flags, provider_address, seekers_address));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

  // Append the public version of the private key to the message so the device
  // can generate the shared secret to decrypt the message.
  const std::optional<std::array<uint8_t, 64>> public_key =
      fast_pair_data_encryptor.GetPublicKey();

  if (public_key) {
    const std::vector<uint8_t> public_key_vec = std::vector<uint8_t>(
        public_key.value().begin(), public_key.value().end());
    data_to_write_vec.insert(data_to_write_vec.end(), public_key_vec.begin(),
                             public_key_vec.end());
  }

  // Write public address request to the keybased characteristic and get
  // response.
  WriteKeyBasedCharacteristic(
      std::string(data_to_write_vec.begin(), data_to_write_vec.end()));
  }

void FastPairGattServiceClientImpl::WriteKeyBasedCharacteristic(
    absl::string_view request) {
  NEARBY_LOGS(INFO) << __func__ << " :Start to write keybased characteristic.";
  MutexLock lock(&mutex_);
  gatt_client_->CallRemoteFunction(
      kKeyBasedCharacteristicIndex, request,
      [this](absl::StatusOr<absl::string_view> response) {
        if (response.ok()) {
          NEARBY_LOGS(INFO)
              << __func__ << ": key based characteristic value changed.";
          NotifyWriteRequestResult(*response);
        } else {
          NEARBY_LOGS(INFO)
              << __func__ << ": Failed to write the key based characteristic: "
              << response.status();
          PairFailure failure =
              absl::IsDeadlineExceeded(response.status())
                  ? PairFailure::kKeyBasedPairingResponseTimeout
                  : PairFailure::kKeyBasedPairingCharacteristicWrite;
          NotifyWriteRequestError(failure);
        }
      });
}

void FastPairGattServiceClientImpl::WritePasskeyAsync(
    uint8_t message_type, uint32_t passkey,
    const FastPairDataEncryptor& fast_pair_data_encryptor,
    WriteResponseCallback callback) {
  CHECK(is_initialized_);
  CHECK(message_type == kSeekerPasskey);

  passkey_write_response_callback_ = std::move(callback);
  const std::array<uint8_t, kAesBlockByteSize> data_to_write =
      fast_pair_data_encryptor.EncryptBytes(
          CreatePasskeyBlock(message_type, passkey));
  std::vector<uint8_t> data_to_write_vec(data_to_write.begin(),
                                         data_to_write.end());

    // Write passkey confirmation request to the passkey characteristic
    WritePasskeyCharacteristic(
        std::string(data_to_write_vec.begin(), data_to_write_vec.end()));
  }

void FastPairGattServiceClientImpl::WritePasskeyCharacteristic(
    absl::string_view request) {
    MutexLock lock(&mutex_);
    gatt_client_->CallRemoteFunction(
        kPasskeyCharacteristicIndex, request,
        [this](absl::StatusOr<absl::string_view> response) {
          if (response.ok()) {
            NEARBY_LOGS(INFO)
                << __func__ << ": Passkey characteristic value changed.";
            NotifyWritePasskeyResult(*response);
          } else {
            NEARBY_LOGS(INFO)
                << __func__ << ": Failed to write the passkey characteristic "
                << response.status();
            PairFailure failure =
                absl::IsDeadlineExceeded(response.status())
                    ? PairFailure::kPasskeyResponseTimeout
                    : PairFailure::kPasskeyPairingCharacteristicWrite;
            NotifyWritePasskeyError(failure);
          }
        });
}

void FastPairGattServiceClientImpl::WriteAccountKey(
    const FastPairDataEncryptor& fast_pair_data_encryptor,
    WriteAccountkeyCallback write_accountkey_callback) {
    MutexLock lock(&mutex_);
    CHECK(is_initialized_);
    account_key_write_callback_ = std::move(write_accountkey_callback);
    std::array<uint8_t, kAesBlockByteSize> raw_account_key =
        CreateAccountKeyBlock();
    const std::array<uint8_t, kAesBlockByteSize> data_to_write =
        fast_pair_data_encryptor.EncryptBytes(raw_account_key);
    gatt_client_->WriteCharacteristic(
        kAccountKeyCharacteristicIndex,
        std::string(data_to_write.begin(), data_to_write.end()),
        api::ble_v2::GattClient::WriteType::kWithResponse,
        [this, account_key =
                   std::string(raw_account_key.begin(), raw_account_key.end())](
            absl::Status status) {
          if (status.ok()) {
            NEARBY_LOGS(INFO)
                << __func__
                << ": Successfully write the accoutkey characteristic.";
            NotifyWriteAccountKeyResult(AccountKey(account_key));
          } else {
            NEARBY_LOGS(INFO)
                << __func__ << ": Failed to write the passkey characteristic ";
            NotifyWriteAccountKeyError(
                PairFailure::kAccountKeyCharacteristicWrite);
          }
        });
}

void FastPairGattServiceClientImpl::NotifyInitializedError(
    PairFailure failure) {
  NEARBY_LOGS(VERBOSE) << __func__ << failure;
  ClearCurrentState();

  executor_->Execute("init-error", [this, failure]() {
    if (on_gatt_initialized_callback_) {
      NEARBY_LOGS(VERBOSE) << __func__ << "Executing initialized callback";
      std::move(on_gatt_initialized_callback_)(failure);
    }
  });
}

void FastPairGattServiceClientImpl::NotifyWriteRequestResult(
    absl::string_view value, std::optional<PairFailure> failure) {
  NEARBY_LOGS(VERBOSE) << __func__;
  executor_->Execute(
      "key-based-response", [this, value = std::string(value), failure]() {
        if (key_based_write_response_callback_) {
          std::move(key_based_write_response_callback_)(value, failure);
        }
      });
}

void FastPairGattServiceClientImpl::NotifyWritePasskeyResult(
    absl::string_view value, std::optional<PairFailure> failure) {
  NEARBY_LOGS(VERBOSE) << __func__;
  executor_->Execute(
      "passkey-response", [this, value = std::string(value), failure]() {
        if (passkey_write_response_callback_) {
          std::move(passkey_write_response_callback_)(value, failure);
        }
      });
}

void FastPairGattServiceClientImpl::NotifyWriteAccountKeyResult(
    std::optional<AccountKey> account_key, std::optional<PairFailure> failure) {
  NEARBY_LOGS(VERBOSE) << __func__;
  executor_->Execute("passkey-response", [this, account_key, failure]() {
    if (account_key_write_callback_) {
      std::move(account_key_write_callback_)(account_key, failure);
    }
  });
}

void FastPairGattServiceClientImpl::ClearCurrentState() {
  MutexLock lock(&mutex_);
  executor_->Execute("clear-current-state",
                    [this, gatt_client = std::move(gatt_client_)]() mutable {
                      if (gatt_client != nullptr) {
                        gatt_client->Stop();
                        defunct_gatt_client_ = std::move(gatt_client);
                      }
                    });
}

}  // namespace fastpair
}  // namespace nearby
