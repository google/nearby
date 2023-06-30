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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_H_

#include <memory>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/fast_pair_controller.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"

namespace nearby {
namespace fastpair {

class Retroactive {
 public:
  class PairingState {
   public:
    explicit PairingState(Retroactive* retro) : retro_(retro) {}
    virtual void Enter() = 0;
    virtual void Exit() = 0;
    virtual ~PairingState() = default;
    Retroactive* retro_;
  };
  enum class PairingStep {
    kNone,
    kNewDevicePairedOrConnected,
    kOpenMessageStream,
    kWaitForModelIdAndBleAddress,
    kFetchAntiSpoofingKey,
    kOpenGattConnection,
    kSendKeyBasedPairingRequest,
    kSendAccountKeyToProvider,
    kAskForUserConfirmation,
    kUploadAccountKeyToCloud,
    kSuccess,
    kFailed
  };

  explicit Retroactive(FastPairController* controller) noexcept;
  ~Retroactive();

  Future<absl::Status> Pair();

  void SetPairingStep(PairingStep step);
  void ExitPairingStep(PairingStep step);
  FastPairController* controller_;
  std::unique_ptr<PairingState> state_;
  PairingStep pairing_step_ = PairingStep::kNone;
  FastPairController::MessageStreamConnectionObserver
      message_stream_connection_observer_;
  FastPairController::BleAddressRotationObserver address_rotation_observer_;
  FastPairController::ModelIdObserver model_id_observer_;
  Future<absl::Status> pairing_result_;
  Future<std::shared_ptr<FastPairDataEncryptor>> data_encryptor_;
  Future<FastPairController::GattClientRef> gatt_client_;
  SingleThreadExecutor executor_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_RETROACTIVE_RETROACTIVE_H_
