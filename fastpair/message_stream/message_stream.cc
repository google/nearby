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

#include "fastpair/message_stream/message_stream.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/bluetooth_utils.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr uint8_t kCompanionInstalledBit = 0x02;
constexpr uint8_t kSupportsSilenceBit = 0x01;
// The default active components response.
constexpr uint8_t kDefaultComponents = 0;
constexpr int kModelIdSize = 3;
constexpr int kBleAddressSize = 6;
// We don't accept more than `kMaxBatteryLevels` battery values from the
// provider.
constexpr int kMaxBatteryLevels = 3;
constexpr int kUnknownBatteryLevel = 0x7F;
constexpr absl::Duration kActiveComponentsTimeout = absl::Seconds(1);
constexpr absl::Duration kRingAckTimeout = absl::Seconds(1);

MessageStream::BatteryInfo ConvertBatteryInfo(uint8_t battery_value) {
  // The highest bit represents `is_charging`, the rest the battery level (in
  // %).
  bool is_charging = battery_value & 0x80;
  int level = battery_value & 0x7F;
  MessageStream::BatteryInfo battery_info{.is_charging = is_charging};
  if (level != kUnknownBatteryLevel) {
    battery_info.level = level;
  }
  return battery_info;
}
}  // namespace

MessageStream::MessageStream(const FastPairDevice& device,
                             std::optional<BluetoothClassicMedium*> bt_classic,
                             Observer& observer)
    : observer_(observer), medium_(device, bt_classic, *this) {}

absl::Status MessageStream::OpenRfcomm() { return medium_.OpenRfcomm(); }

absl::Status MessageStream::OpenL2cap(absl::string_view ble_address) {
  return medium_.OpenL2cap(ble_address);
}

absl::Status MessageStream::Disconnect() { return medium_.Disconnect(); }

Future<uint8_t> MessageStream::GetActiveComponents() {
  Future<uint8_t> future(kActiveComponentsTimeout);
  absl::Status status = medium_.Send(
      Message{.message_group = MessageGroup::kDeviceInformationEvent,
              .message_code = MessageCode::kActiveComponentRequest});
  if (status.ok()) {
    MutexLock lock(&mutex_);
    if (get_active_components_request_) {
      get_active_components_request_->SetException({Exception::kInterrupted});
    }
    get_active_components_request_ = std::make_unique<Future<uint8_t>>(future);
  } else {
    future.SetException({Exception::kIo});
  }
  return future;
}

absl::Status MessageStream::SendCapabilities(bool companion_app_installed,
                                             bool supports_silence_mode) {
  uint8_t capabilites = 0;
  if (companion_app_installed) {
    capabilites |= kCompanionInstalledBit;
  }
  if (supports_silence_mode) {
    capabilites |= kSupportsSilenceBit;
  }
  return medium_.Send(
      Message{.message_group = MessageGroup::kDeviceInformationEvent,
              .message_code = MessageCode::kCapabilites,
              .payload = {capabilites}});
}

absl::Status MessageStream::SendPlatformType(
    api::DeviceInfo::OsType platform_type, uint8_t custom_code) {
  return medium_.Send(
      Message{.message_group = MessageGroup::kDeviceInformationEvent,
              .message_code = MessageCode::kPlatformType,
              .payload = {static_cast<uint8_t>(platform_type), custom_code}});
}

// Asks the Provider to ring.
// Returns true if the Provider replies with an ACK.
Future<bool> MessageStream::Ring(uint8_t components, absl::Duration duration) {
  constexpr MessageGroup kGroup = MessageGroup::kDeviceActionEvent;
  constexpr MessageCode kCode = MessageCode::kRing;
  Future<bool> future(kRingAckTimeout);
  std::string payload = {components};
  uint8_t minutes = absl::ToInt64Minutes(duration);
  if (minutes != 0) {
    payload.append({minutes});
  }
  {
    MutexLock lock(&mutex_);
    waiting_for_ack_.push_back(MessageWithAck{
        .message_group = kGroup, .message_code = kCode, .future = future});
  }
  absl::Status status = medium_.Send(Message{
      .message_group = kGroup, .message_code = kCode, .payload = payload});
  if (!status.ok()) {
    FinishCall(kGroup, kCode, false);
  }
  return future;
}

void MessageStream::FinishCall(MessageGroup group, MessageCode code,
                               bool result) {
  NEARBY_LOGS(INFO) << "Finish call " << static_cast<int>(group) << ", "
                    << static_cast<int>(code) << " with result: " << result;
  MutexLock lock(&mutex_);
  auto it = std::find_if(waiting_for_ack_.begin(), waiting_for_ack_.end(),
                         [&](const MessageWithAck& item) {
                           return item.message_group == group &&
                                  item.message_code == code;
                         });
  if (it != waiting_for_ack_.end()) {
    NEARBY_LOGS(INFO) << "Finishing call with result: " << result;
    it->future.Set(result);
    waiting_for_ack_.erase(it);
  }
}

// Medium::Observer
void MessageStream::OnConnectionResult(absl::Status result) {
  observer_.OnConnectionResult(result);
}
void MessageStream::OnDisconnected(absl::Status status) {
  observer_.OnDisconnected(status);
}

void MessageStream::OnReceived(Message message) {
  bool handled = false;
  NEARBY_LOGS(INFO) << "Received: " << message;
  switch (message.message_group) {
    case MessageGroup::kAcknowledgement:
      handled = HandleAcknowledgement(message);
      break;
    case MessageGroup::kBluetooth:
      handled = HandleBluetooth(message);
      break;
    case MessageGroup::kCompanionAppEvent:
      handled = HandleCompanionAppEvent(message);
      break;
    case MessageGroup::kDeviceInformationEvent:
      handled = HandleDeviceInformationEvent(message);
      break;
    case MessageGroup::kDeviceActionEvent:
      handled = HandleDeviceActionEvent(message);
      break;
    case MessageGroup::kSass:
      break;
    default:
      break;
  }
  if (!handled) {
    NEARBY_LOGS(INFO) << "Unrecognized " << message;
  }
}

bool MessageStream::HandleDeviceInformationEvent(const Message& message) {
  switch (message.message_code) {
    case MessageCode::kModelId: {
      if (message.payload.size() != kModelIdSize) {
        NEARBY_LOGS(WARNING) << "Model id event size should be " << kModelIdSize
                             << " but is " << message.payload.size();
        break;
      }
      observer_.OnModelId(message.payload);
      return true;
    }
    case MessageCode::kBleAddressUpdated: {
      if (message.payload.size() != kBleAddressSize) {
        NEARBY_LOGS(WARNING)
            << "BLE address updated event size should be " << kBleAddressSize
            << " but is " << message.payload.size();
        break;
      }
      ByteArray address(message.payload);
      observer_.OnBleAddressUpdated(BluetoothUtils::ToString(address));
      return true;
    }
    case MessageCode::kBatteryUpdated: {
      if (message.payload.size() > kMaxBatteryLevels) {
        NEARBY_LOGS(WARNING)
            << "Battery level event size should be <= " << kMaxBatteryLevels
            << " but is " << message.payload.size();
        break;
      }
      std::vector<BatteryInfo> battery_levels(message.payload.size());
      for (size_t i = 0; i < message.payload.size(); i++) {
        battery_levels[i] = ConvertBatteryInfo(message.payload[i]);
      }
      observer_.OnBatteryUpdated(std::move(battery_levels));
      return true;
    }
    case MessageCode::kRemainingBatteryTime: {
      int battery_time = 0;
      if (message.payload.size() == 1) {
        battery_time = static_cast<uint8_t>(message.payload[0]);
      } else if (message.payload.size() == 2) {
        battery_time = 256 * static_cast<uint8_t>(message.payload[0]) +
                       static_cast<uint8_t>(message.payload[1]);
      } else {
        NEARBY_LOGS(WARNING)
            << "Remaining battery event size should be 1 or 2 bytes but is "
            << message.payload.size();
        break;
      }
      observer_.OnRemainingBatteryTime(absl::Minutes(battery_time));
      return true;
    }
    case MessageCode::kActiveComponentResponse: {
      uint8_t components = message.payload.size() == 1
                               ? message.payload.data()[0]
                               : kDefaultComponents;
      MutexLock lock(&mutex_);
      if (get_active_components_request_) {
        get_active_components_request_->Set(components);
        get_active_components_request_.reset();
      }
      return true;
    }
    default:
      break;
  }
  return false;
}

bool MessageStream::HandleAcknowledgement(const Message& message) {
  bool result;
  if (message.message_code == MessageCode::kAck) {
    result = true;
  } else if (message.message_code == MessageCode::kNack) {
    result = false;
  } else {
    return false;
  }
  // The payload of ACK/NACK message contains the group/code of the original
  // message.
  if (message.payload.size() < 2) {
    NEARBY_LOGS(INFO) << "ACK/NACK too short: " << message;
    return false;
  }
  MessageGroup group = static_cast<MessageGroup>(message.payload[0]);
  MessageCode code = static_cast<MessageCode>(message.payload[1]);
  FinishCall(group, code, result);
  return true;
}

bool MessageStream::HandleBluetooth(const Message& message) {
  switch (message.message_code) {
    case MessageCode::kEnableSilenceMode:
      observer_.OnEnableSilenceMode(true);
      return true;
    case MessageCode::kDisableSilenceMode:
      observer_.OnEnableSilenceMode(false);
      return true;
    default:
      break;
  }
  return false;
}

bool MessageStream::HandleCompanionAppEvent(const Message& message) {
  switch (message.message_code) {
    case MessageCode::kLogBufferFull:
      observer_.OnLogBufferFull();
      return true;
    default:
      break;
  }
  return false;
}

bool MessageStream::HandleDeviceActionEvent(const Message& message) {
  switch (message.message_code) {
    case MessageCode::kRing: {
      uint8_t components = 0;
      absl::Duration duration = absl::ZeroDuration();
      if (!message.payload.empty()) {
        components = static_cast<uint8_t>(message.payload[0]);
      }
      if (message.payload.size() >= 2) {
        duration = absl::Minutes(static_cast<uint8_t>(message.payload[1]));
      }
      bool result = observer_.OnRing(components, duration);
      SendAcknowledgement(message, result);
      return true;
    }
    default:
      break;
  }
  return false;
}

void MessageStream::SendAcknowledgement(const Message& message, bool ack) {
  absl::Status status = medium_.Send(
      Message{.message_group = MessageGroup::kAcknowledgement,
              .message_code = ack ? MessageCode::kAck : MessageCode::kNack,
              .payload = {static_cast<uint8_t>(message.message_group),
                          static_cast<uint8_t>(message.message_code)}});
  if (!status.ok()) {
    NEARBY_LOGS(WARNING) << "Failed to send ACK/NACK " << status;
  }
}
}  // namespace fastpair
}  // namespace nearby
