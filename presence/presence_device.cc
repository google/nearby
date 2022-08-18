// Copyright 2020 Google LLC
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

#include "presence/presence_device.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

#include "absl/random/random.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace presence {
namespace {
using ::location::nearby::SystemClock;

std::string GenerateRandomEndpointId() {
  absl::BitGen gen;

  std::string rand_endpoint_id(4, 'x');
  for (char &ch : rand_endpoint_id) {
    ch = kEndpointIdChars[absl::Uniform(gen, 0, kEndpointIdCharsSize)];
  }
  return rand_endpoint_id;
}

uint64_t GeneratePresenceId() {
  absl::BitGen gen;
  return absl::uniform_int_distribution<uint64_t>(0, UINT64_MAX - 1)(gen);
}

}  // namespace

bool PresenceDevice::IsEndpointIdValid() {
  if (endpoint_id_.size() != kEndpointIdLength) {
    return false;
  }
  return std::count_if(endpoint_id_.begin(), endpoint_id_.end(),
                       [](unsigned char c) { return std::isalnum(c); }) ==
         kEndpointIdLength;
}

absl::Status PresenceDevice::InitPresenceDevice() {
  if (device_initialized_) {
    // Don't edit the PresenceDevice, just say all good. If device_initialized_
    // is true, then we have already gotten the object into a good state.
    return absl::OkStatus();
  }
  if (!IsEndpointIdValid()) {
    NEARBY_LOGS(WARNING) << "No/Invalid Endpoint ID was specified, generating.";
    endpoint_id_ = GenerateRandomEndpointId();
  }
  presence_id_ = GeneratePresenceId();
  discovery_time_ = absl::ToUnixMillis(SystemClock::ElapsedRealtime());
  absl::Status status = ValidatePresenceDevice();
  // make sure that we do not accidentally approve the object for use when it
  // may not be in a usable state.
  device_initialized_ = status.ok();
  return status;
}

absl::StatusOr<std::string> PresenceDevice::GetDeviceName() const {
  if (!device_initialized_) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Init must be called before using this function.");
  }
  if (name_.empty()) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Invalid Device Name");
  }
  return name_;
}

absl::StatusOr<PresenceDevice::PresenceDeviceType>
PresenceDevice::GetPresenceDeviceType() const {
  if (!device_initialized_) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Init must be called before using this function.");
  }
  if (type_ > PresenceDeviceType::kWatch) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Invalid PresenceDeviceType");
  }
  return type_;
}

absl::StatusOr<PresenceDevice::MotionType> PresenceDevice::GetMotionType()
    const {
  if (!device_initialized_) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Init must be called before using this function.");
  }
  if (motion_type_ > MotionType::kPointAndHold) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Invalid MotionType");
  }
  return motion_type_;
}

absl::StatusOr<std::string> PresenceDevice::GetEndpointId() const {
  if (!device_initialized_) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Init must be called before using this function.");
  }

  return endpoint_id_;
}

absl::StatusOr<float> PresenceDevice::GetConfidence() const {
  if (!device_initialized_) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Init must be called before using this function.");
  }
  if (confidence_ < 0.0f) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Invalid confidence");
  }
  return confidence_;
}

absl::Status PresenceDevice::ValidatePresenceDevice() {
  if (name_.empty()) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "Device name cannot be empty");
  }
  return absl::OkStatus();
}

absl::Span<ConnectionInfo *> PresenceDevice::GetConnectionInfos() {
  return absl::Span<ConnectionInfo *>(connection_infos_.data(),
                                      connection_infos_.size());
}

}  // namespace presence
}  // namespace nearby
