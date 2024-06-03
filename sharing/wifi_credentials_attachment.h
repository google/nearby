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

#ifndef THIRD_PARTY_NEARBY_SHARING_WIFI_CREDENTIALS_ATTACHMENT_H_
#define THIRD_PARTY_NEARBY_SHARING_WIFI_CREDENTIALS_ATTACHMENT_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "sharing/attachment.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {

// Represents a WiFi credentials attachment.
class WifiCredentialsAttachment : public Attachment {
 public:
  using SecurityType =
      nearby::sharing::service::proto::WifiCredentialsMetadata::SecurityType;

  WifiCredentialsAttachment(std::string ssid, SecurityType security_type,
                            std::string password = "", bool is_hidden = false,
                            int32_t batch_id = 0,
                            SourceType source_type = SourceType::kUnknown);
  WifiCredentialsAttachment(int64_t id, std::string ssid,
                            SecurityType security_type,
                            std::string password = "", bool is_hidden = false,
                            int32_t batch_id = 0,
                            SourceType source_type = SourceType::kUnknown);
  WifiCredentialsAttachment(const WifiCredentialsAttachment&) = default;
  WifiCredentialsAttachment(WifiCredentialsAttachment&&) = default;
  WifiCredentialsAttachment& operator=(const WifiCredentialsAttachment&) =
      default;
  WifiCredentialsAttachment& operator=(WifiCredentialsAttachment&&) = default;
  ~WifiCredentialsAttachment() override = default;

  absl::string_view ssid() const { return ssid_; }
  SecurityType security_type() const { return security_type_; }
  absl::string_view password() const { return password_; }
  bool is_hidden() const { return is_hidden_; }

  // Attachment:
  absl::string_view GetDescription() const override;
  ShareType GetShareType() const override;

  void set_password(std::string password);
  void set_is_hidden(bool is_hidden);

 private:
  std::string ssid_;
  SecurityType security_type_;
  std::string password_;
  bool is_hidden_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_WIFI_CREDENTIALS_ATTACHMENT_H_
