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

#include "fastpair/handshake/fast_pair_handshake_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"
#include "fastpair/handshake/fast_pair_gatt_service_client_impl.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/logging.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

FastPairHandshakeImpl::FastPairHandshakeImpl(FastPairDevice& device,
                                             Mediums& mediums,
                                             OnCompleteCallback on_complete,
                                             SingleThreadExecutor* executor)
    : FastPairHandshake(std::move(on_complete), nullptr, nullptr) {
  fast_pair_gatt_service_client_ =
      FastPairGattServiceClientImpl::Factory::Create(device, mediums, executor);
  fast_pair_gatt_service_client_->InitializeGattConnection(
      [&](std::optional<PairFailure> failure) {
        OnGattClientInitializedCallback(device, failure);
      });
}

void FastPairHandshakeImpl::OnGattClientInitializedCallback(
    FastPairDevice& device, std::optional<PairFailure> failure) {
  if (failure.has_value()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Failed to init gatt client with failure = "
                         << failure.value();
    std::move(on_complete_callback_)(device, failure.value());
    return;
  }

  NEARBY_LOGS(INFO)
      << __func__
      << ": Fast Pair GATT service client initialization successful.";
  FastPairDataEncryptorImpl::Factory::CreateAsync(
      device,
      [&](std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
        OnDataEncryptorCreateAsync(device, std::move(fast_pair_data_encryptor));
      });
}

void FastPairHandshakeImpl::OnDataEncryptorCreateAsync(
    FastPairDevice& device,
    std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
  if (!fast_pair_data_encryptor) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Failed to create Fast Pair Data Encryptor.";
    std::move(on_complete_callback_)(device,
                                     PairFailure::kDataEncryptorRetrieval);
    return;
  }

  fast_pair_data_encryptor_ = std::move(fast_pair_data_encryptor);
  NEARBY_LOGS(INFO) << __func__ << ": Beginning key-based pairing protocol";
  fast_pair_gatt_service_client_->WriteRequestAsync(
      /*message_type=*/kKeyBasedPairingType,
      /*flags=*/kInitialOrSubsequentFlags,
      /*provider_address=*/device.GetBleAddress(),
      /*seekers_address=*/"", *fast_pair_data_encryptor_,
      [&](absl::string_view response, std::optional<PairFailure> failure) {
        OnWriteResponse(device, response, failure);
      });
}

void FastPairHandshakeImpl::OnWriteResponse(
    FastPairDevice& device, absl::string_view response,
    std::optional<PairFailure> failure) {
  if (failure.has_value()) {
    NEARBY_LOGS(WARNING)
        << __func__
        << ": Failed during key-based pairing protocol with failure = "
        << failure.value();
    std::move(on_complete_callback_)(device, failure.value());
    return;
  }

  NEARBY_LOGS(INFO) << __func__ << ": Successfully wrote response.";

  if (response.size() != kAesBlockByteSize) {
    NEARBY_LOGS(WARNING)
        << __func__ << ": Handshake failed because of incorrect response size.";
    std::move(on_complete_callback_)(
        device, PairFailure::kKeybasedPairingResponseDecryptFailure);
    return;
  }

  std::vector<uint8_t> response_bytes(response.begin(), response.end());
  fast_pair_data_encryptor_->ParseDecryptResponse(
      response_bytes, [&](std::optional<DecryptedResponse> response) {
        OnParseDecryptedResponse(device, response);
      });
}

void FastPairHandshakeImpl::OnParseDecryptedResponse(
    FastPairDevice& device, std::optional<DecryptedResponse>& response) {
  if (!response.has_value()) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Missing decrypted response from parse.";
    std::move(on_complete_callback_)(
        device, PairFailure::kKeybasedPairingResponseDecryptFailure);
    return;
  }
  NEARBY_LOGS(INFO) << __func__
                    << ": Successfully decrypted and parsed response.";

  device.SetPublicAddress(
      device::CanonicalizeBluetoothAddress(response->address_bytes));
  completed_successfully_ = true;
  std::move(on_complete_callback_)(device, std::nullopt);
}

}  // namespace fastpair
}  // namespace nearby
