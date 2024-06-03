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

#include "sharing/wifi_credentials_attachment.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {

WifiCredentialsAttachment::WifiCredentialsAttachment(
    std::string ssid, SecurityType security_type, std::string password,
    bool is_hidden, int32_t batch_id, SourceType source_type)
    : Attachment(Attachment::Family::kWifiCredentials, ssid.size(), batch_id,
                 source_type),
      ssid_(ssid),
      security_type_(security_type),
      password_(std::move(password)),
      is_hidden_(is_hidden) {}

WifiCredentialsAttachment::WifiCredentialsAttachment(
    int64_t id, std::string ssid, SecurityType security_type,
    std::string password, bool is_hidden, int32_t batch_id,
    SourceType source_type)
    : Attachment(id, Attachment::Family::kWifiCredentials, ssid.size(),
                 batch_id, source_type),
      ssid_(ssid),
      security_type_(security_type),
      password_(std::move(password)),
      is_hidden_(is_hidden) {}

absl::string_view WifiCredentialsAttachment::GetDescription() const {
  return ssid_;
}

ShareType WifiCredentialsAttachment::GetShareType() const {
  return ShareType::kWifiCredentials;
}

void WifiCredentialsAttachment::set_password(std::string password) {
  password_ = std::move(password);
}

void WifiCredentialsAttachment::set_is_hidden(bool is_hidden) {
  is_hidden_ = is_hidden;
}

}  // namespace sharing
}  // namespace nearby
