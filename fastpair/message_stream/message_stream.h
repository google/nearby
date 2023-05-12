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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MESSAGE_STREAM_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MESSAGE_STREAM_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "fastpair/message_stream/medium.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace fastpair {

class MessageStream : public Medium::Observer {
 public:
  struct BatteryInfo {
    bool is_charging;
    std::optional<int> level;
  };
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnConnectionResult(absl::Status result) = 0;

    virtual void OnDisconnected(absl::Status status) = 0;

    // `void' callbacks don't send acknowledgements to the Provider.
    // 'bool' callbacks send ACK or NACK depending on the return value.
    virtual void OnEnableSilenceMode(bool enable) = 0;

    virtual void OnLogBufferFull() = 0;

    virtual void OnModelId(absl::string_view model_id) = 0;

    // `address` is in canonical, human-readable format.
    virtual void OnBleAddressUpdated(absl::string_view address) = 0;

    virtual void OnBatteryUpdated(std::vector<BatteryInfo> battery_levels) = 0;

    virtual void OnRemainingBatteryTime(absl::Duration duration) = 0;

    // Note, there are no callbacks for Active Components Request and Active
    // Components Response. Use `GetActiveComponents()` instead.

    virtual bool OnRing(uint8_t components, absl::Duration duration) = 0;
  };

  MessageStream(const FastPairDevice& device,
                std::optional<BluetoothClassicMedium*> bt_classic,
                Observer& observer);
  MessageStream(MessageStream&& other) = default;
  absl::Status OpenRfcomm();

  absl::Status OpenL2cap(absl::string_view ble_address);

  absl::Status Disconnect();

  // Asks the Provider for active components.
  // Returns the active components bitmap.
  Future<uint8_t> GetActiveComponents();

  absl::Status SendCapabilities(bool companion_app_installed,
                                bool supports_silence_mode);

  absl::Status SendPlatformType(api::DeviceInfo::OsType platform_type,
                                uint8_t custom_code);

  // Asks the Provider to ring.
  // Returns true if the Provider replies with an ACK.
  Future<bool> Ring(uint8_t components, absl::Duration duration);

  // Medium::Observer
  void OnConnectionResult(absl::Status result) override;

  void OnDisconnected(absl::Status status) override;

  void OnReceived(Message message) override;

 private:
  // Notifies the caller that message {group, code} was handled by the provider
  // with the `result`.
  void FinishCall(MessageGroup group, MessageCode code, bool result);
  bool HandleBluetooth(const Message& message);
  bool HandleCompanionAppEvent(const Message& message);
  bool HandleDeviceActionEvent(const Message& message);
  bool HandleAcknowledgement(const Message& message);
  bool HandleDeviceInformationEvent(const Message& message);
  void SendAcknowledgement(const Message& message, bool ack);

  struct MessageWithAck {
    MessageGroup message_group;
    MessageCode message_code;
    Future<bool> future;
  };
  // A list of messages waiting for ACK/NACK.
  std::vector<MessageWithAck> waiting_for_ack_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<Future<uint8_t>> get_active_components_request_
      ABSL_GUARDED_BY(mutex_);
  Mutex mutex_;
  Observer& observer_;
  Medium medium_;
};

}  // namespace fastpair
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MESSAGE_STREAM_H_
