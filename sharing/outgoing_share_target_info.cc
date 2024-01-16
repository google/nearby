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

#include "sharing/outgoing_share_target_info.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

OutgoingShareTargetInfo::OutgoingShareTargetInfo() = default;

OutgoingShareTargetInfo::OutgoingShareTargetInfo(OutgoingShareTargetInfo&&) =
    default;

OutgoingShareTargetInfo& OutgoingShareTargetInfo::operator=(
    OutgoingShareTargetInfo&&) = default;

OutgoingShareTargetInfo::~OutgoingShareTargetInfo() = default;

std::vector<Payload> OutgoingShareTargetInfo::ExtractTextPayloads() {
  return std::move(text_payloads_);
}

std::vector<Payload> OutgoingShareTargetInfo::ExtractFilePayloads() {
  return std::move(file_payloads_);
}

std::optional<Payload> OutgoingShareTargetInfo::ExtractNextPayload() {
  if (!text_payloads_.empty()) {
    Payload payload = text_payloads_.back();
    text_payloads_.pop_back();
    return payload;
  }

  if (!file_payloads_.empty()) {
    Payload payload = file_payloads_.back();
    file_payloads_.pop_back();
    return payload;
  }

  if (!wifi_credentials_payloads_.empty()) {
    Payload payload = wifi_credentials_payloads_.back();
    wifi_credentials_payloads_.pop_back();
    return payload;
  }

  return std::nullopt;
}

}  // namespace sharing
}  // namespace nearby
