// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_CONSTANTS_H_
#define THIRD_PARTY_NEARBY_SHARING_CONSTANTS_H_

#include <cstdint>

#include "absl/time/time.h"

namespace nearby {
namespace sharing {

// Timeout for reading a response frame from the remote device.
constexpr absl::Duration kReadResponseFrameTimeout = absl::Seconds(60);

// Timeout for initiating a connection to a remote device.
constexpr absl::Duration kInitiateNearbyConnectionTimeout = absl::Seconds(60);

// The delay before the sender will disconnect from the receiver after sending a
// file. Note that the receiver is expected to immediately disconnect, so this
// delay is a worst-effort disconnection. Disconnecting too early might
// interrupt in flight packets, especially over Wi-Fi LAN.
constexpr absl::Duration kOutgoingDisconnectionDelay = absl::Seconds(60);

// The delay before the receiver will disconnect from the sender after rejecting
// an incoming file. The sender is expected to disconnect immediately after
// reading the rejection frame.
constexpr absl::Duration kIncomingRejectionDelay = absl::Seconds(2);

// The delay before the initiator of the cancellation will disconnect from the
// other device. The device that did not initiate the cancellation is expected
// to disconnect immediately after reading the cancellation frame.
constexpr absl::Duration kInitiatorCancelDelay = absl::Seconds(5);

// Timeout for reading a frame from the remote device.
constexpr absl::Duration kReadFramesTimeout = absl::Seconds(15);

// Time to delay running the task to invalidate send and receive surfaces.
constexpr absl::Duration kInvalidateDelay = absl::Milliseconds(500);

// Time between successive progress updates.
constexpr absl::Duration kMinProgressUpdateFrequency = absl::Milliseconds(100);

// Attachments size threshold for transferring high quality medium. The default
// value is 1MB to match the default setting on Android.
constexpr int64_t kAttachmentsSizeThresholdOverHighQualityMedium = 1000000;

// If true, the user will be able to accept incoming Wi-Fi Credential
// attachments and join the network when the attachment is opened.
constexpr bool kSupportReceivingWifiCredentials = true;

// Time between successive instantaneous transfer speed in seconds.
constexpr double kTransferSpeedUpdateInterval = 1.0;

// Time between successive transfer completion ETA in seconds.
constexpr double kEstimatedTimeRemainingUpdateInterval = 3.0;

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_CONSTANTS_H_
